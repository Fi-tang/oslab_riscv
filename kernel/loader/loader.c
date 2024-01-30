#include <os/string.h>
#include <os/kernel.h>
#include <os/loader.h> // newly added!
#include <type.h>

# define TASK_MEM_PROMPT_BASE 0x53000000
# define PROMPT_OFFSET        0x01000000

uint64_t load_task_img_first_alloc(int task_id){
        long task4_in_memory_address = TASK_MEM_PROMPT_BASE + (task_id - 1) * TASK_SIZE;
        long task4_in_memory_offset_address = task4_in_memory_address - tasks[task_id].task_blockstart_offset;
        long task4_block_number = tasks[task_id].total_block_num;
        long task4_block_id = tasks[task_id].start_block_id;
        bios_sd_read(task4_in_memory_offset_address, task4_block_number, task4_block_id);
        return task4_in_memory_address;
}

uint64_t load_task_img_by_name(int tasknum, char *taskname){
    for(int i = 0; i < (tasknum + 1); i++){
        if(strcmp(taskname , tasks[i].taskname) == 0 ){
            long task4_in_memory_address = TASK_MEM_BASE + (i - 1) * TASK_SIZE;
            uint64_t first_prompt_address = load_task_img_first_alloc(i);
            memcpy(task4_in_memory_address, first_prompt_address, tasks[i].task_filesz);
            return task4_in_memory_address;
        }
    }
    return 0;
}

//*********************Virtual memory *********************************************************//
//*********************************************load task image*********************************************
/**
function: first load task image, after fullfill it into user_pgtable
*/
void load_task_image(char *taskname, PTE *user_level_one_pgdir){
    printl("\n\n[load_task_image]: %s\n", taskname);
    /**
    Step 1. record corresponding task-related parameter.
    */
    int task_id = -1;
    for(int i = 0; i < TASK_MAXNUM; i++){
        if(strcmp(tasks[i].taskname, taskname) == 0){
            task_id = i;
            break;
        }
    }
    int start_block_id = tasks[task_id].start_block_id;
    int total_block_num = tasks[task_id].total_block_num;
    int max_block_id = start_block_id + total_block_num - 1;    // judge it is a legal sector to visit
    long task_filesz = tasks[task_id].task_filesz;
    long task_memorysz = tasks[task_id].task_memorysz;
    long task_blockstart_offset = tasks[task_id].task_blockstart_offset;

    printl("start_block_id=%d\ttotal_block_num=%d\ttask_filesz=%ld\n", start_block_id, total_block_num, task_filesz);
    printl("task_memorysz=%ld\ttask_blockstart_offset=%ld\n",task_memorysz, task_blockstart_offset);
    printl("max_block_id = %d\n", max_block_id);
    /**
    according to the load function,
    we need a free buffer to temoprarily place the first 512B, then copy it to the corresponding position.
    @free_buffer_page: place first and the 8th block data here, than move it to somewhereelse(according to offset)
    */
    /*
    Step 2: allocate temp_buffer to store first 512 B  --> fix it
    */
    printl("free_buffer_address: 0x%x\n", free_buffer_address);  // actually, it is kva: 0xffffffc052001000

    /**
    Step 3: allocate > memorysz's Byte and record it into array
    */
    int total_page_num = ROUND(task_memorysz, PAGE_SIZE) / PAGE_SIZE;
    printl("total_page_num = %d\n", total_page_num);

    uintptr_t task_page_array[total_page_num];
    for(int i = 0; i < total_page_num; i++){
        task_page_array[i] = (uintptr_t)kmalloc();
    }

    for(int i = 0; i < total_page_num; i++){
        printl("task_page_array[%d]=0x%x ", i, task_page_array[i]);     // debug line
    }

    /**
    Step 4: formally load task image
    Every time, we load one page
    */
    for(int i = 0; i < total_page_num; i++){
        /**
        note: every time, we deal with 8 block,
        the 1st block should be moved to free_buffer_address, then memcpy here

        bios_sd_read(unsigned mem_address, unsigned num_of_blocks, unsigned block_id);
        void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len)

        [512 - offset] |  [offset][1][2][3][4][5][6][512 - offset]  | [offset]

        1. bios_sd_read(free_buffer_address, 1, start_block_id)
        2. memcpy(task_page_array[i], free_buffer_address + offset, 512 - offset)
        3. bios_sd_read(task_page_array[i] + (512 - offset), 7, start_block_id + 1)
        4. bios_sd_read(free_buffer_address, 1, start_block_id + 8)
        5. memcpy(task_page_array[i] + 4096 - offset, free_buffer_address, offset)
        start_block_id = start_block_id + 8
        */
        bios_sd_read(free_buffer_address, 1, start_block_id);
        memcpy(task_page_array[i], free_buffer_address + task_blockstart_offset, 512 - task_blockstart_offset);
        /*
        if left block is less than 7, it will not be necessary
        */
        if(start_block_id + 7 > max_block_id){
            printl("\n[Final round]: This is final round!\n");
            printl("visit [%d] , only need to visit %d blocks\n", start_block_id + 1, (max_block_id - start_block_id));
            bios_sd_read(task_page_array[i] + (512 - task_blockstart_offset), (max_block_id - start_block_id), start_block_id + 1);
        }
        else{
            printl("\n[Normal round]: This is a normal round\n");
            printl("This round visit [%d][%d][%d][%d][%d][%d][%d]\n", start_block_id + 1, start_block_id + 2, start_block_id + 3, 
            start_block_id + 4, start_block_id + 5, start_block_id + 6, start_block_id + 7);
            bios_sd_read(task_page_array[i] + (512 - task_blockstart_offset), 7, start_block_id + 1);

            bios_sd_read(free_buffer_address, 1, start_block_id + 8);
            memcpy(task_page_array[i] + PAGE_SIZE - task_blockstart_offset, free_buffer_address, task_blockstart_offset);
            start_block_id = start_block_id + 8;
        }
    }
    /**Step 5: the last page, we need to clean task_filesz to task_memsz, fill it with 0*/
    int task_filesz_spare = task_filesz - (total_page_num - 1) * PAGE_SIZE;
    int task_memorysz_spare = task_memorysz - (total_page_num - 1) * PAGE_SIZE;
    printl("\ntask_filesz_spare: %d\ttask_memorysz_spare: %d\n", task_filesz_spare, task_memorysz_spare);
    
    memset(task_page_array[total_page_num - 1] + task_filesz_spare, 0, PAGE_SIZE - task_filesz_spare);
    /**Step 6: fullfill page_table*/
    clear_pgdir(free_buffer_address);
    Build_user_page_table(task_id, user_level_one_pgdir, task_page_array);
}

//************************Building user page table********************************************************
/* after load task image, we need to fullfill user page_table
(1) virtual address ranges
(2) page allocate situation
*/
void Build_user_page_table(int task_id, PTE *user_level_one_pgdir, uintptr_t *task_page_array){
    // Step 1: get related virtual address ranges: from 0x10000 to (0x10000 + tasks[i].task_memorysz)
    int task_memorysz = tasks[task_id].task_memorysz;
    printl("\n\n[Build_user_page_table]: \n");
    printl("virtual_address start from 0x10000 to 0x%x\n",(0x10000lu + task_memorysz));  // memorysz = 5456 = 0x1550, 0x10000 ~ 0x11550

    int total_page_num = ROUND(task_memorysz, PAGE_SIZE) / PAGE_SIZE;
    for(int i = 0; i < total_page_num; i++){
        uintptr_t kva_load_address = task_page_array[i];
        printl("%d's kva_load_address = 0x%x\n", i, kva_load_address);
        uint64_t va = 0x10000lu + i * PAGE_SIZE;
        uint64_t pa = kva2pa(kva_load_address);
        map_single_user_page(va, pa, user_level_one_pgdir);
    }
}