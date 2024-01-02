#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
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

void Implement_Memcpy(void *dst, void *src, long size){
    char *dst_ptr, *src_ptr;
    if(dst == NULL || src == NULL){
        return;
    }
    dst_ptr = dst;
    src_ptr = src;
    while(size--){
        *dst_ptr++ = *src_ptr++;
    }
}

uint64_t load_task_img_by_name(int tasknum, char *taskname){
    for(int i = 0; i < (tasknum + 1); i++){
        if(strcmp(taskname , tasks[i].taskname) == 0 ){
            long task4_in_memory_address = TASK_MEM_BASE + (i - 1) * TASK_SIZE;
            uint64_t first_prompt_address = load_task_img_first_alloc(i);
            Implement_Memcpy(task4_in_memory_address, first_prompt_address, tasks[i].task_filesz);
            return task4_in_memory_address;
        }
    }
    return 0;
}

long load_task_img_filesz(int tasknum, char *taskname){
    for(int i = 0; i < (tasknum + 1); i++){
        if(strcmp(taskname, tasks[i].taskname) == 0){
            return tasks[i].task_filesz;
        }
    }
    return 0;
}

long load_task_img_memorysz(int tasknum, char *taskname){
    for(int i = 0; i < (tasknum + 1); i++){
        if(strcmp(taskname, tasks[i].taskname) == 0){
            return tasks[i].task_memorysz;
        }
    }
    return 0;
}