#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/string.h> // newly added!

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

void do_scheduler(void){
    check_sleeping();  // First check sleep queue!
    int current_cpu = get_current_cpu_id();

    pcb_t *prev_running = global_cpu[current_cpu].cpu_current_running;
    // only let useful pcb_information Enque ready_queue!
    if(strcmp(prev_running -> name, "pid0") != 0 && strcmp(prev_running -> name, "pid1") != 0 && prev_running -> status == TASK_RUNNING){
        prev_running -> status = TASK_READY;
        Enque_FromTail(&ready_queue, &(prev_running -> list));
    }
    if(strcmp(prev_running -> name, "pid0") == 0 || strcmp(prev_running -> name, "pid1") == 0){
        prev_running -> status = TASK_READY;
    }

    int ready_queue_num = CountNum_AccordList(&ready_queue);
    if(ready_queue_num == 0){
        global_cpu[current_cpu].cpu_current_running = &pcb[current_cpu];
    }
    else{
        /**
        need to check the next running pcb's mask
        */
        list_head *has_find_suitable_node = NULL;
        for(int i = 0; i < ready_queue_num; i++){
            list_head *deque_node = Deque_FromHead(&ready_queue);
            pcb_t *deque_pcb_node = GetPcb_FromList(deque_node);
            int current_pcb_mask = deque_pcb_node -> pcb_mask;
            Enque_FromTail(&ready_queue, deque_node);
            if(current_pcb_mask == 3){
                has_find_suitable_node = deque_node;
                break;
            }
            else if((current_pcb_mask == 2 && current_cpu == 1) || (current_pcb_mask == 1 && current_cpu == 0)){
                has_find_suitable_node = deque_node;
                break;
            }
        }
        if(has_find_suitable_node == NULL){
            global_cpu[current_cpu].cpu_current_running = &pcb[current_cpu];
        }
        else{
            DequeNode_AccordList(&ready_queue, has_find_suitable_node);
            pcb_t *target_pcb_node = GetPcb_FromList(has_find_suitable_node);
            global_cpu[current_cpu].cpu_current_running = target_pcb_node;
        }
    }
    global_cpu[current_cpu].cpu_current_running -> status = TASK_RUNNING;
    switch_to(prev_running, global_cpu[current_cpu].cpu_current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    global_cpu[get_current_cpu_id()].cpu_current_running -> wakeup_time = sleep_time * 100;         // only change for debug, later change to 1000
    do_block(&(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &sleep_queue);
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
            int current_cpu = (global_cpu[0].cpu_current_running == &pcb[i])? 0 : 1;
            printk("[%d]\tPID\t:\t%d\t%s\t\tSTATUS\t:\t%s\t\tmask:\t0x%x\t\tRunning on core %d\n",
            count, pcb[i].pid, pcb[i].name, "RUNNING", pcb[i].pcb_mask, current_cpu);
            count++;
        }
        else if(pcb[i].status == TASK_BLOCKED){
            printk("[%d]\tPID\t:\t%d\t%s\t\tSTATUS\t:\t%s\t\tmask:\t0x%x\n",
            count, pcb[i].pid, pcb[i].name, "BLOCKED", pcb[i].pcb_mask);
            count++;
        }
        else if(pcb[i].status == TASK_READY){
            printk("[%d]\tPID\t:\t%d\t%s\t\tSTATUS\t:\t%s\t\tmask:\t0x%x\n",
            count, pcb[i].pid, pcb[i].name, "READY  ", pcb[i].pcb_mask);
            count++;
        }
    }
}

pid_t do_getpid(){
    return global_cpu[get_current_cpu_id()].cpu_current_running -> pid;
}


int do_waitpid(pid_t pid){
    // Enque current_running to wait_queue
    printl("[DO-Waitpid]: %d %s is waiting %d %s\n", global_cpu[get_current_cpu_id()].cpu_current_running -> pid,
    global_cpu[get_current_cpu_id()].cpu_current_running -> name, pid,pcb[pid].name);
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].pid == pid && pcb[i].status != TASK_EXITED){
            do_block(&(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &(pcb[i].wait_list));
            return pid;
        }
    }
    return 0;
}

/**
the following are kill-releated function
*/
void kill_release_lock(pid_t pid){
    for(int k = 0; k < LOCK_NUM; k++){
        if(mlocks[k].lock_owner == &(pcb[pid])){
            do_mutex_lock_release(k);
        }
    }
}


void kill_release_pcb_on_waitpid(pid_t pid){
    list_head *target_head = &(pcb[pid].wait_list);
    while(target_head -> next != target_head){
        list_head *deque_node = Deque_FromHead(&(pcb[pid].wait_list));
        do_unblock(deque_node);
    }
}

void kill_release_self_from_all_pcb(pid_t pid){
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(i != pid && pcb[i].status != TASK_EXITED){
            list_head *target_head = &(pcb[i].wait_list);
            if(target_head -> next != target_head){
                if(FindNode_InQueue( &(pcb[i].wait_list) , &(pcb[pid].list)) == 1){
                    DequeNode_AccordList(&(pcb[i].wait_list), &(pcb[pid].list));
                }
            }
        }
    }
}

void kill_release_from_lock_queue(pid_t pid){
    for(int k = 0; k < LOCK_NUM; k++){
        if(mlocks[k].lock_owner != NULL && mlocks[k].lock_owner != &(pcb[pid])){
            list_head *target_head = &(mlocks[k].block_queue);
            if(target_head -> next != target_head){
                if(FindNode_InQueue( &(mlocks[k].block_queue), &(pcb[pid].list)) == 1){
                    DequeNode_AccordList( &(mlocks[k].block_queue), &(pcb[pid].list));
                }
            }
        }
    }
}

void kill_release_from_barrier(pid_t pid){
    for(int i = 0; i < BARRIER_NUM; i++){
        if(global_barrier[i].barrier_key != 0){
            list_head *target_head = &(global_barrier[i].barrier_wait_list);
            if(target_head -> next != target_head){
                if(FindNode_InQueue(&(global_barrier[i].barrier_wait_list), &pcb[pid].list) == 1){
                    while(target_head -> next != target_head){
                        list_head *deque_node = Deque_FromHead(&(global_barrier[i].barrier_wait_list));
                        do_unblock(deque_node);                             // this turn, free all
                    }
                    global_barrier[i].target_barrier_num -= 1;              // next turn, only need to count (target_barrier_num - 1)'s process
                }
            }
        }
    }
}

void kill_release_from_semaphore(pid_t pid){
    for(int i = 0; i < SEMAPHORE_NUM; i++){
        if(global_semaphore_resource[i].occupied_or_not == 1){
            list_head *target_head = &(global_semaphore_resource[i].sema_wait_list);
            if(target_head -> next != target_head){
                if(FindNode_InQueue(&(global_semaphore_resource[i].sema_wait_list), &(pcb[pid].list)) == 1){
                    DequeNode_AccordList(&(global_semaphore_resource[i].sema_wait_list), &(pcb[pid].list));
                    // assume only be blocked at one semaphore
                }
            }
        }
    }
}

void kill_release_from_condition(pid_t pid){
    for(int i = 0; i < CONDITION_NUM; i++){
        if(global_condition[i].condition_key != -1){
            list_head *target_head = &(global_condition[i].condition_wait_list);
            if(target_head -> next != target_head){
                if(FindNode_InQueue(&(global_condition[i].condition_wait_list), &(pcb[i].list)) == 1){
                    DequeNode_AccordList(&(global_condition[i].condition_wait_list), &(pcb[pid].list));
                }
            }
        }
    }
}

void kill_release_from_mailbox(pid_t pid){
    for(int i = 0; i < MBOX_NUM; i++){
        if(strcmp(global_mailbox[i].mailbox_name, "") != 0){
            list_head *target_send_head = &(global_mailbox[i].mailbox_send_wait_list);
            if(target_send_head -> next != target_send_head){
                if(FindNode_InQueue( &(global_mailbox[i].mailbox_send_wait_list) , &(pcb[pid].list)) == 1){
                    DequeNode_AccordList(&(global_mailbox[i].mailbox_send_wait_list), &(pcb[pid].list));
                }
            }

            list_head *target_recv_head = &(global_mailbox[i].mailbox_recv_wait_list);
            if(target_recv_head -> next != target_recv_head){
                if(FindNode_InQueue( &(global_mailbox[i].mailbox_recv_wait_list), &(pcb[pid].list) ) == 1){
                    DequeNode_AccordList( &(global_mailbox[i].mailbox_recv_wait_list), &(pcb[pid].list));
                }
            }
        }
    }
}

int do_kill(pid_t pid){
    // the killed pcb is current_running
    // step 1. if it hold locks, free all lock
    kill_release_lock(pid);
    // step 2. if it has pcb blocked on own wait_list
    kill_release_pcb_on_waitpid(pid);
    // step 3. if it is blocked on other pcb
    kill_release_self_from_all_pcb(pid);
    // step 4. if it is blocked on semaphore
    kill_release_from_semaphore(pid);
    // step 5. if it is blocked on barrier
    kill_release_from_barrier(pid);
    // step 6. remove it from condition
    kill_release_from_condition(pid);
    // step 7. remove it from ready_queue or sleep_queue
    if(FindNode_InQueue(&ready_queue , &(pcb[pid].list)) == 1){
        DequeNode_AccordList(&ready_queue, &(pcb[pid].list));
    }
    if(FindNode_InQueue(&sleep_queue, &(pcb[pid].list)) == 1){
        DequeNode_AccordList(&sleep_queue, &(pcb[pid].list));
    }
    
    // step 8. remove it from other lock's wait_queue
    kill_release_from_lock_queue(pid);
    // step 9. remove it from mailbox's send or receive wait list
    kill_release_from_mailbox(pid);

    pcb[pid].status = TASK_EXITED;

    return 1;
}

void do_exit(void){
    // first check, do I have un-released locks?
    printl("[Exit]: %d - %s exited!\n", global_cpu[get_current_cpu_id()].cpu_current_running -> pid, global_cpu[get_current_cpu_id()].cpu_current_running -> name);
    global_cpu[get_current_cpu_id()].cpu_current_running -> status = TASK_EXITED;
    kill_release_lock(global_cpu[get_current_cpu_id()].cpu_current_running -> pid);
    // second, free all locked pcb
    kill_release_pcb_on_waitpid(global_cpu[get_current_cpu_id()].cpu_current_running -> pid);
    printl("Before final exit:\n");
    PrintPcb_FromList(&(global_cpu[get_current_cpu_id()].cpu_current_running -> wait_list));

    if(FindNode_InQueue(&ready_queue, &(global_cpu[get_current_cpu_id()].cpu_current_running -> list)) == 1){
        DequeNode_AccordList(&ready_queue, &(global_cpu[get_current_cpu_id()].cpu_current_running -> list));
    }
}


//************************[P3-task4] taskset *********************************************************
void do_taskset(int mask, char *taskname, int task_pid){
    if(strcmp(taskname, "") == 0){
        // means it is already started
        pcb[task_pid].pcb_mask = mask;
    }
    else{
        // need to start this task
        int started_pid = do_exec(taskname, 1, NULL);   // The first argv[0] is taskname, we can put it here or leave it somewhere else
        pcb[started_pid].pcb_mask = mask;
    }
}