#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/string.h> // newly added!
#include <csr.h>       // newly added!
#include <os/loader.h> // newly added!

#define BOOT_LOADER_ADDRESS 0xffffffc050200000
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define current_task_entry_address 0x100000lu 
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
extern void ret_from_exception();
//************************debugging line *****************************************
// use list to find the whole pcb
pcb_t *GetPcb_FromList(list_head *node){
   unsigned long list_offset = (unsigned long) (&((pcb_t *)0)-> list);
   pcb_t *return_pcb = NULL;
   return_pcb = (pcb_t *) ((char *)(node) - list_offset);
   return return_pcb;
}

// use wait_queue to find the whole pcb
pcb_t *GetPcb_FromWaitList(list_head *node){
    unsigned long list_offset = (unsigned long) (&((pcb_t *)0)-> wait_list);
    pcb_t *return_pcb = NULL;
    return_pcb = (pcb_t *) ((char *)(node) - list_offset);
    return return_pcb;
}

void PrintPcb_FromList(list_head *head){
    if(head -> next == head){
        printl("NULL\n");
        return;
    }
    else{
        if(head == &sleep_queue){
            list_head *node = head -> next;
            while(node != head){
                pcb_t *print_pcb_list = GetPcb_FromList(node);
                printl("[%d]: %s [left %d seconds]-> ", print_pcb_list -> pid, print_pcb_list -> name
                , print_pcb_list -> wakeup_time);
                node = node -> next;
            }
            printl("NULL\n");
        }
        else{
            list_head *node = head -> next;
            while(node != head){
                pcb_t *print_pcb_list = GetPcb_FromList(node);
                printl("[%d]: %s  ", print_pcb_list -> pid, print_pcb_list -> name);
                if(print_pcb_list -> status == TASK_BLOCKED){
                    printl(" TASK_BLOCKED -> ");
                }
                else if(print_pcb_list -> status == TASK_READY){
                    printl(" TASK_READY -> ");
                }
                else if(print_pcb_list -> status == TASK_RUNNING){
                    printl(" TASK_RUNNING -> ");
                }
                else if(print_pcb_list -> status == TASK_EXITED){
                    printl(" TASK_EXITED -> ");
                }
                node = node -> next;
            }
            printl("NULL\n");
        }
    }
}

//******************Initialize registers***********************************************************
/************************************************************/
void init_pcb_regs(switchto_context_t *kernel_switchto_context, regs_context_t *user_regs_context, pcb_t *pcb, ptr_t entry_point){
    // ********************* kernel ***************************//
    kernel_switchto_context -> regs[0] = ret_from_exception; // ra
    kernel_switchto_context -> regs[1] = pcb -> kernel_sp; // sp

    //********************** user ****************************//
    user_regs_context -> regs[1] = entry_point;  // entry_point 
    user_regs_context -> regs[2] = pcb -> user_sp;
    user_regs_context -> regs[4] = pcb;

    user_regs_context -> sstatus = SR_SPIE | SR_SUM;
    user_regs_context -> sepc = entry_point;        // entry_point // restore context, first happen because in ret_from_exception:[sepc + 4]
    user_regs_context -> sbadaddr = 0;
    user_regs_context -> scause = SCAUSE_IRQ_FLAG + IRQ_S_TIMER;
    user_regs_context -> regs_pointer = &(pcb -> pcb_user_regs_context);
}

void do_writeArgvToMemory(pcb_t *pcb, int argc, char *argv[]){
    if(argc == 0 || (argc == 1 && argv == NULL)){
        pcb -> pcb_user_regs_context.regs[10] = argc;   // need to pass the assert(argc >= 1) test, where argv[0] == taskname
        pcb -> pcb_user_regs_context.regs[11] = 0x0;
        return;
    }   
    // need 8 bytes to allocate argv[0] - argv[1] - ... - argv[n]
    reg_t avail_user_stack = pcb -> user_sp;
    int count_mem_usage = 0;
    
    char *filled_with_zero = NULL;
    for(int i = 0; i < 8; i++){
        filled_with_zero = (char *)(pcb -> user_sp - i);
        *filled_with_zero = '\0';
    }
    for(int i = 0; i <= argc; i++){
        avail_user_stack -= 8;
        count_mem_usage += 8;
    }
    
    for(int i = 0; i < argc; i++){
        char **unassigned_location = NULL;
        unassigned_location = (char **)(pcb -> user_sp - (argc - i) * 8);
                        // assign arv[0] --> 'test_barrier'
        int total_number = strlen(argv[i]) + 1;

        avail_user_stack -= total_number;

        char *string_mem = NULL;
        string_mem = (char *)(avail_user_stack + 1);

        strncpy(string_mem, argv[i], strlen(argv[i]));

        *unassigned_location = string_mem;
        count_mem_usage += total_number;
    }

    // write argc and argv to a0 and a1 register!
    pcb -> pcb_user_regs_context.regs[10] = argc;
    pcb -> pcb_user_regs_context.regs[11] = (pcb -> user_sp - (argc * 8));

    pcb -> user_sp = pcb -> user_sp - count_mem_usage;
    while(pcb -> user_sp % 128 != 0){
        pcb -> user_sp -= 1;
    }
}

void assign_initial_pcb(char *name, int alloc_index){
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    pcb[alloc_index].kernel_sp = allocKernelStack();      // need to notice that kernel_sp allocate from 0xffffffc052001000
    pcb[alloc_index].user_sp = 0xf00010000lu;

    pcb[alloc_index].kernel_stack_base = pcb[alloc_index].kernel_sp;
    pcb[alloc_index].user_stack_base = pcb[alloc_index].user_sp;

    pcb[alloc_index].cursor_x = alloc_index;
    pcb[alloc_index].cursor_y = alloc_index;
    Initialize_QueueNode(&pcb[alloc_index].list);
    Initialize_QueueNode(&pcb[alloc_index].wait_list);

    pcb[alloc_index].pid = alloc_index;
    strcpy(pcb[alloc_index].name, name);

    do_load_virtual_task_img_by_name(pcb[alloc_index].name, &pcb[alloc_index]);

    pcb[alloc_index].status = TASK_RUNNING;
    pcb[alloc_index].pcb_mask = 0x3;
    init_pcb_regs(&pcb[alloc_index].pcb_switchto_context, &pcb[alloc_index].pcb_user_regs_context, &pcb[alloc_index], current_task_entry_address);
}

// virtual load_task_image
void do_load_virtual_task_img_by_name(char *taskname, pcb_t *pcb){
    // Step 1: allocate pcb[i]'s user_page
    printl("\n\n[do_load_virtual_task_img_by_name]: \n");
    allocate_user_pgdir(pcb);
    printl("pcb[%d]'s user_pgdir: 0x%x\n", pcb -> pid, pcb -> user_pgdir_kva);

    // Step 2: fill kernel information
    copy_kernel_pgdir_to_user_pgdir(pa2kva(PGDIR_PA), pcb -> user_pgdir_kva);
    PTE *user_level_one_pgdir = (PTE *)(pcb -> user_pgdir_kva);
    load_task_image(taskname, user_level_one_pgdir);
    allocUserStack(user_level_one_pgdir);       // use a free page as user_stack
}


void init_pcb_loop(void){  // cpu [0] always point to pid0, cpu [1] always point to pid1
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    Initialize_QueueNode(&ready_queue); 
    for(int i = 0; i <= task_num; i++){
        if(strcmp(tasks[i].taskname, "pid0") == 0){
            assign_initial_pcb(tasks[i].taskname, 0);
        }
        if(strcmp(tasks[i].taskname, "pid1") == 0){
            assign_initial_pcb(tasks[i].taskname, 1);
        }
    }

    for(int i = 2; i < NUM_MAX_TASK; i++){
        pcb[i].pid = i;
        pcb[i].status = TASK_EXITED;
    }
    do_exec("shell", 0, NULL);
}

pid_t do_exec(char *name, int argc, char *argv[]){
    //***********************************************************
    // the argv[] parameter now is all correct!
    //**********************debugging part************************
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].status == TASK_EXITED){
            pcb[i].kernel_sp = allocKernelStack();
            pcb[i].user_sp = 0xf00010000lu;

            pcb[i].kernel_stack_base = pcb[i].kernel_sp;
            pcb[i].user_stack_base = pcb[i].user_sp;

            pcb[i].cursor_x = i;
            pcb[i].cursor_y = i;
            Initialize_QueueNode(&pcb[i].list);
            Initialize_QueueNode(&pcb[i].wait_list);

            strcpy(pcb[i].name, name);
            
            do_load_virtual_task_img_by_name(pcb[i].name, &pcb[i]);
            pcb[i].status = TASK_READY;
             
            // If otherwise declared, inhereit father's mask
            int current_cpu = get_current_cpu_id();
            pcb_t *father_pcb_node = global_cpu[current_cpu].cpu_current_running;
            // default mask from father
            pcb[i].pcb_mask = father_pcb_node -> pcb_mask;

            do_writeArgvToMemory(&pcb[i], argc, argv);

            init_pcb_regs(&pcb[i].pcb_switchto_context, &pcb[i].pcb_user_regs_context, &pcb[i], current_task_entry_address);

            Enque_FromTail(&ready_queue, &pcb[i].list);
            return i;
        }
    }
    return 0;
}

//************virtual memory*****************************
// Step1: allocate user_pgdir(corresponding to level_one_pgdir)
void allocate_user_pgdir(pcb_t *pcb){
    struct SentienlNode *malloc_user_pgdir_sentienl = (struct SentienlNode *)kmalloc(1 * PAGE_SIZE);
    printl("allocate_user_pgdir: ");
    print_page_alloc_info(malloc_user_pgdir_sentienl);

    pcb -> user_pgdir_kva = (uintptr_t)(malloc_user_pgdir_sentienl -> head);
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!![Warning]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // kmalloc(1 * PAGE_SIZE) is free to clear_pgdir, but need to pay attention to the usage of next pointer, we may further need it
}
//*************************************************debugging line end****************************

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
    // newly added, used for changing user_pgdir
    set_satp(SATP_MODE_SV39, (global_cpu[current_cpu].cpu_current_running -> pid) + 1, kva2pa(global_cpu[current_cpu].cpu_current_running -> user_pgdir_kva) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
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

