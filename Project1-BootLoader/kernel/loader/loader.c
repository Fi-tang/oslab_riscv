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