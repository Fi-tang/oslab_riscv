#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>


uint64_t load_task_img_by_name(int tasknum, char *taskname){
    for(int i = 0; i < (tasknum + 1); i++){
        if(strcmp(taskname , tasks[i].taskname) == 0 ){
            long task4_in_memory_address = TASK_MEM_BASE + (i - 1) * TASK_SIZE;
            long task4_in_memory_offset_address = task4_in_memory_address - tasks[i].task_blockstart_offset;
            long task4_block_number = tasks[i].total_block_num;
            long task4_block_id = tasks[i].start_block_id;
            bios_sd_read(task4_in_memory_offset_address, task4_block_number, task4_block_id);
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