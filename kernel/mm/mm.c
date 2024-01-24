#include <os/mm.h>
#include <os/task.h>   // newly added!
#include <os/string.h> // newly added!
#include <os/kernel.h> // newly added!

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void kinit(){   // allocate from 0xffffffc052002000 to 0xfffffc060000000
    global_free_sentienl = (struct SentienlNode *)AVAILABLE_KERNEL;
    malloc_free_sentienl = (struct SentienlNode *)(AVAILABLE_KERNEL + (PAGE_SIZE / 2));
    
    struct ListNode *prev_node = (struct ListNode *)(global_free_sentienl -> head);

    for(uint64_t index = AVAILABLE_KERNEL + PAGE_SIZE; index < FREEMEM_KERNEL_END; index += PAGE_SIZE){
        uint64_t listnode_address = index;
        struct ListNode *current_node = (struct ListNode *)listnode_address;
        current_node -> next = NULL;
        if(index == AVAILABLE_KERNEL + PAGE_SIZE){
            global_free_sentienl -> head = current_node;
            prev_node = global_free_sentienl -> head;
        }
        else{
            prev_node -> next = current_node;
            prev_node = current_node;
        }
    }
}


void freePage(ptr_t baseAddr)   // assume 4 KB
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    ptr_t round_down_baseAddr = ROUNDDOWN(baseAddr, PAGE_SIZE);

    struct ListNode *current_node = (struct ListNode *)round_down_baseAddr;
    current_node -> next = global_free_sentienl -> head;
    global_free_sentienl -> head = current_node;  // insert from head
}

void *kmalloc(size_t size) // remember pass number * PAGE_SIZE
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    static bool init_or_not = false;
    if(init_or_not == false){
        kinit();
        init_or_not = true;
    }

    if(size == 0) return NULL;
    // Step1:cacluate num of page
    uint64_t round_up_alloc = ROUND(size, PAGE_SIZE);
    int page_num = round_up_alloc / PAGE_SIZE;

    // Step2: allocate new SentienlNode
    // place it at half of the page
    malloc_free_sentienl -> head = NULL;

    // Step3: traverse
    struct ListNode *global_temp = global_free_sentienl -> head;
    struct ListNode *prev_node = NULL;
    while(global_temp != NULL && page_num > 0){
        if(malloc_free_sentienl -> head == NULL){
            malloc_free_sentienl -> head = global_temp;
        }
        prev_node = global_temp;                // find the last prev to cut off 
        global_temp = global_temp -> next;      // global_free_sentienl ->[] -> [] -> [] -> [] -> [] | -> [] -> [] -> []
        page_num--;
    }
    prev_node -> next = NULL;
    global_free_sentienl -> head = global_temp;
    return malloc_free_sentienl;
}

void print_page_alloc_info(struct SentienlNode *sentienl_head){
    struct ListNode *temp = sentienl_head -> head;
    while(temp != NULL){
        printl("0x%x -> ", (uintptr_t)temp);
        temp = temp -> next;
    }
    printl("NULL\n");
}


/**
this function is to map three level user_page
(1) level_one_pgdir, has been allocated
(2) level_two_pgdir, need to call kmalloc(1 * PAGE_SIZE)
(3) level_three_pgdir, need to call kmalloc(2 * PAGE_SIZE)

debug error: 
*********** first record *****************************************************
because we assign every page as struct ListNode *, 
so level_one_pgdir 0xffffffc052003000 looks like: 0x52003000 00000000 00000000 00000000
it did not pass the test that level_one_pgdir[vpn2] == 0

// usage: map_single_user_page(0x100000, 0x54000000, (PTE *)(pcb[0].user_pgdir_kva));
*/
void map_single_user_page(uint64_t va, uint64_t pa, PTE *level_one_pgdir){
    printl("mapping 0x%x to 0x%x\n", va, pa);
    printl("[map_single_user_page]: level_one_pgdir 0x%x\n", (uintptr_t)level_one_pgdir); // debug

    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn1 << PPN_BITS) ^ (va >> NORMAL_PAGE_SHIFT);

    if(level_one_pgdir[vpn2] == 0){ // have not allocated level_two_pgdir
        struct SentienlNode *malloc_level_two = (struct SentienlNode *)kmalloc(1 * PAGE_SIZE);
        print_page_alloc_info(malloc_level_two);         // debug
        uint64_t return_level_two_address = (uint64_t)(malloc_level_two -> head);
        printl("[map_single_user_page]: level_two_pgdir 0x%x\n", kva2pa(return_level_two_address)); // debug
        set_pfn(&level_one_pgdir[vpn2], kva2pa(return_level_two_address) >> NORMAL_PAGE_SHIFT);
        set_attribute(&level_one_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(level_one_pgdir[vpn2])));     // clean level_two_pgdir, for furture fullfill
    }
    PTE *level_two_pgdir = (PTE *)pa2kva(get_pa(level_one_pgdir[vpn2]));
    if(level_two_pgdir[vpn1] == 0){  // have not allocated level_three_pgdir
        struct SentienlNode *malloc_level_three = (struct SentienlNode *)kmalloc(1 * PAGE_SIZE);
        print_page_alloc_info(malloc_level_three);     // debug
        uint64_t return_level_three_address = (uint64_t)(malloc_level_three -> head);
        printl("[map_single_user_page]: level_three_pgdir 0x%x\n", kva2pa(return_level_three_address)); // debug
        set_pfn(&level_two_pgdir[vpn1], kva2pa(return_level_three_address) >> NORMAL_PAGE_SHIFT);
        set_attribute(&level_two_pgdir[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(level_two_pgdir[vpn1])));     // clean level_three_pgdir, for furture fullfill
    }
    PTE *level_three_pgdir = (PTE *)pa2kva(get_pa(level_two_pgdir[vpn1]));
    set_pfn(&level_three_pgdir[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &level_three_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);

    printl("[Verify]level_one_pgdir: 0x%x\n", kva2pa(level_one_pgdir));
    printl("[Verify]level_two_pgdir: 0x%x,\tfunction[vpn2:%d] result = 0x%x\n", kva2pa(level_two_pgdir), vpn2, get_pa(level_one_pgdir[vpn2]));
    printl("[Verify]level_three_pgdir: 0x%x,\tfunction[vpn1:%d] result = 0x%x\n", kva2pa(level_three_pgdir), vpn1, get_pa(level_two_pgdir[vpn1]));
    printl("[Verify]physical_frame: 0x%x,\tfunction[vpn0:%d] result = 0x%x\n\n\n", pa, vpn0, get_pa(level_three_pgdir[vpn0]));
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}

//*********************************************load task image*********************************************
/**
function: first load task image, after fullfill it into user_pgtable
*/
void load_task_image(char *taskname){
    printl("\n\n[load_task_image]: \n");
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
    long task_filesz = tasks[task_id].task_filesz;
    long task_memorysz = tasks[task_id].task_memorysz;
    long task_blockstart_offset = tasks[task_id].task_blockstart_offset;

    printl("start_block_id=%d\ttotal_block_num=%d\ttask_filesz=%ld\n", start_block_id, total_block_num, task_filesz);
    printl("task_memorysz=%ld\ttask_blockstart_offset=%ld\n",task_memorysz, task_blockstart_offset);
    /**
    according to the load function,
    we need a free buffer to temoprarily place the first 512B, then copy it to the corresponding position.
    @free_buffer_page: place first and the 8th block data here, than move it to somewhereelse(according to offset)
    */
    /*
    Step 2: allocate temp_buffer to store first 512 B
    */
    struct SentienlNode *free_buffer_page = (struct SentienlNode *)kmalloc(1 * PAGE_SIZE);  
    printl("\nfree_temp_block:");
    print_page_alloc_info(free_buffer_page);
    uintptr_t free_buffer_address = (uintptr_t)(free_buffer_page -> head); 
    printl("free_buffer_address: 0x%x\n", free_buffer_page -> head);  // actually, it is kva: 0xffffffc052004000

    /**
    Step 3: allocate > memorysz's Byte and record it into array
    */
    struct SentienlNode *task_page = (struct SentienlNode *)kmalloc(task_memorysz);
    printl("\ntask_page:");
    print_page_alloc_info(task_page);

    int total_page_num = ROUND(task_memorysz, PAGE_SIZE) / PAGE_SIZE;
    printl("total_page_num = %d\n", total_page_num);

    struct ListNode *temp_task_page = task_page -> head;
    uintptr_t task_page_array[total_page_num];
    for(int i = 0; i < total_page_num; i++){
        task_page_array[i] = (uintptr_t)temp_task_page;
        temp_task_page = temp_task_page -> next;
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
        bios_sd_read(task_page_array[i] + (512 - task_blockstart_offset), 7, start_block_id + 1);
        bios_sd_read(free_buffer_address, 1, start_block_id + 8);
        memcpy(task_page_array[i] + PAGE_SIZE - task_blockstart_offset, free_buffer_address, task_blockstart_offset);
        start_block_id = start_block_id + 8;
    }
    /**Step 5: the last page, we need to clean task_filesz to task_memsz, fill it with 0*/
}