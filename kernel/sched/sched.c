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
    // printl("[DO_SCHEDULER]: enter do_scheduler\n");
    check_sleeping();
    list_head *deque_node = Deque_FromHead(&ready_queue);
    if(deque_node == NULL){
        return;     // when ready_queue is empty! need to solve it!
    }
    else{
        pcb_t *deque_pcb_node = GetPcb_FromList(deque_node);
        if(deque_pcb_node -> status == TASK_BLOCKED || deque_pcb_node -> status == TASK_EXITED){
            return;
        }
        else{
            Enque_FromTail(&ready_queue, deque_node);
            pcb_t *prev_running = current_running;
            current_running = deque_pcb_node;

            // make sure that pid0 never return or exited
            if(strcmp(prev_running -> name, "main") == 0){
                prev_running -> status = TASK_EXITED;
                do_exec("pid0", 0, NULL);
            }

            if(strcmp(prev_running -> name, "main") != 0 && prev_running -> status == TASK_RUNNING){
                prev_running -> status = TASK_READY;
            }
            current_running -> status = TASK_RUNNING;
            switch_to(prev_running, deque_pcb_node);
        }
    }

    // TODO: [p2-task1] switch_to current_running
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running -> wakeup_time = sleep_time * 100;         // only change for debug, later change to 1000
    do_block(&(current_running -> list), &sleep_queue);
}

/**
pcb_node: always mean current_running -> list
queue: means the queue to be blocked in
*/
void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    if(FindNode_InQueue(&ready_queue, pcb_node) == 1){
        DequeNode_AccordList(&ready_queue, pcb_node);
        // printk("\n++++++++++++ The BLOCKED pcb [%s] +++++\n", current_running -> name);
    }
    pcb_t *get_block_pcb = GetPcb_FromList(pcb_node);
    get_block_pcb -> status = TASK_BLOCKED;
    Enque_FromTail(queue, pcb_node);

    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
     if(FindNode_InQueue(&ready_queue, pcb_node) == 0){
        // printk("\n******************* The UNBLOCKED pcb [%s] **************\n", pcb_unblock_node -> name);
        pcb_t *pcb_unblock_node = GetPcb_FromList(pcb_node);
        pcb_unblock_node -> status = TASK_READY;
        Enque_FromTail(&ready_queue, pcb_node);
    }
}

void do_process_show(){
    printk("[Process Table]: \n");
    int count = 0;
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].status == TASK_RUNNING){
            printk("[%d]\tPID\t:\t%d\t%s\t\tSTATUS\t:\t%s\n",
            count, pcb[i].pid, pcb[i].name, "RUNNING");
            count++;
        }
        else if(pcb[i].status == TASK_BLOCKED){
            printk("[%d]\tPID\t:\t%d\t%s\tSTATUS\t:\t%s\n",
            count, pcb[i].pid, pcb[i].name, "BLOCKED");
            count++;
        }
        else if(pcb[i].status == TASK_READY){
            printk("[%d]\tPID\t:\t%d\t%s\tSTATUS\t:\t%s\n",
            count, pcb[i].pid, pcb[i].name, "READY");
            count++;
        }
    }
}

pid_t do_getpid(){
    return current_running -> pid;
}


int do_waitpid(pid_t pid){
    // Enque current_running to wait_queue
    printl("[DO-Waitpid]: %d %s is waiting %d %s\n", current_running -> pid,
    current_running -> name, pid,pcb[pid].name);
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid && pcb[i].status != TASK_EXITED){
            do_block(&(current_running -> list), &(pcb[i].wait_list));
            return pid;
        }
    }
    return 0;
}

int do_kill(pid_t pid){     // almost same as do_exit
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid && pcb[i].status != TASK_EXITED){
            pcb[i].status = TASK_EXITED;
            for(int k = 0; k < LOCK_NUM; k++){
                if(mlocks[k].lock_owner == &(pcb[i])){
                    do_mutex_lock_release(k);
                }
            }

            // free pcb_queue !
            list_head *target_head = &(pcb[i].wait_list);
            while(target_head -> next != target_head){
                list_head *deque_node = Deque_FromHead(&(pcb[i].wait_list));
                do_unblock(deque_node);
            }

            if(FindNode_InQueue(&ready_queue, &(pcb[i].list)) == 1){
                DequeNode_AccordList(&ready_queue, &(pcb[i].list));
            }
            if(FindNode_InQueue(&sleep_queue, &(pcb[i].list)) == 1){
                DequeNode_AccordList(&sleep_queue, &(pcb[i].list));
            }
            return 1;
        }
    }
    return 0;
}

void do_exit(void){
    // first check, do I have un-released locks?
    current_running -> status = TASK_EXITED;
    for(int i = 0; i < LOCK_NUM; i++){
        if(mlocks[i].lock_owner == current_running){
            do_mutex_lock_release(i);
        }
    }
    // second, free all locked pcb
    list_head *target_head = &(current_running -> wait_list);
    if(target_head -> next == target_head){
        return;
    }
    else{
        while(target_head -> next != target_head){
            list_head *deque_node = Deque_FromHead(&(current_running -> wait_list));
            do_unblock(deque_node);
        }
    }
    printl("Before final exit:\n");
    PrintPcb_FromList(&(current_running -> wait_list));

    if(FindNode_InQueue(&ready_queue, &(current_running -> list)) == 1){
        DequeNode_AccordList(&ready_queue, &(current_running -> list));
    }
}