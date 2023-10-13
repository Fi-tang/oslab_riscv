#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    printk("\nIN [DO-SCHEDULER]: \n");
    DequeueNodeFromHead(&ready_queue);  // first deque the current, need revise, main should not be placed at ready_queue
    list_head *The_next_to_shedule = DequeueNodeFromHead(&ready_queue);
    if(The_next_to_shedule == NULL){
        printk("\n IN [DO_SCHEDULER]: No process is ready, queue empty!\n");
    }
    else{
        EnqueueNodeFromTail(&ready_queue, current_running);
    }
    pcb_t *prev = current_running;
    pcb_t *current = GetPcbFromList(The_next_to_shedule);
    /**
    error report: prev and current are all main!
    */
    printk("\n IN [DO_SCHEDULER]: ==[prev]==The prev process's id = %d\n", prev -> pid);
    printk("\n IN [DO_SCHEDULER]: ==[prev]==The prev process's name = %s\n", prev -> name);
    printk("\n IN [DO_SCHEDULER]: ==[current]==The current process's id = %d\n", current -> pid);
    printk("\n IN [DO_SCHEDULER]: ==[current]==The current process's name = %s\n", current -> name);

    current_running = current;
    // TODO: [p2-task1] switch_to current_running
    switch_to(prev, current);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
}
