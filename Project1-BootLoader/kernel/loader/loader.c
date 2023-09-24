#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    // [p1-task3]
    /**
     * @debugrecording: 
     * input '1', 
     * call_bios(which = 11, arg0 = 1378877440, arg1=15, arg2 = 736)
     * ASCII '1' = 49 --> 49 * 15 + 1 = 736
     * where the 736th block does not exist!
     * taskid = taskid - '0', change it in /init/main.c
    */
    long task_in_memory_address = TASK_MEM_BASE + (taskid - 1) * TASK_SIZE;
    long task3_block_number = 15;
    long task3_block_id = 15 * taskid + 1;
    bios_sd_read(task_in_memory_address, task3_block_number, task3_block_id);

    return task_in_memory_address;
}

// [p1-task4]
uint64_t load_task_img_by_name(int tasknum, char *taskname){
    for(int i = 0; i < (tasknum + 1); i++){
        if(strcmp(taskname , tasks[i].taskname) == 0 ){
            long task4_in_memory_address = TASK_MEM_BASE + (i - 1) * TASK_SIZE;
            long task4_block_number = tasks[i].total_block_num;
            long task4_block_id = tasks[i].start_block_id;
            bios_sd_read(task4_in_memory_address, task4_block_number, task4_block_id);
            return task4_in_memory_address;
        }
    }
    return 0;
}