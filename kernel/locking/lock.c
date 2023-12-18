#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <printk.h> // newly added 

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++){
        spin_lock_init(&(mlocks[i].lock));
        Initialize_QueueNode(&(mlocks[i].block_queue));
        mlocks[i].key = 0;
        mlocks[i].lock_owner = NULL;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock -> status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    if(lock -> status == UNLOCKED){
        return 1;
    }
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    lock -> status = LOCKED;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock -> status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for(int i = 0; i < LOCK_NUM; i++){
        if(mlocks[i].key == key){
            return i;
        }
    }
    for(int i = 0; i < LOCK_NUM; i++){
        if(mlocks[i].key == 0){
            mlocks[i].key = key;
            return i;
        }
    }
    return 0;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    /**
    first it format like:
    if(){}
    else{
        do_block();
        do_mutex_lock_acquire(mlock_idx);
    }
    change the recursion to the while(1) loop, in case that the stack can be too deep!
    */
    while(1){
        if(spin_lock_try_acquire(&(mlocks[mlock_idx].lock)) == 1){
            mlocks[mlock_idx].lock_owner = global_cpu[get_current_cpu_id()].cpu_current_running;
            spin_lock_acquire(&(mlocks[mlock_idx].lock));
            break;
        }
        else{
            do_block(&(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &(mlocks[mlock_idx].block_queue));
        }
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    // printk("\n[LOCK_RELEASE]: %s release lock\n",current_running -> name);
    spin_lock_release(&(mlocks[mlock_idx].lock));
    mlocks[mlock_idx].lock_owner = NULL;
    list_head *target_head = &(mlocks[mlock_idx].block_queue);
    if(target_head -> next == target_head){
        return;
    }
    else{
        while(target_head -> next != target_head){
            list_head *deque_node = Deque_FromHead(&(mlocks[mlock_idx].block_queue));
            do_unblock(deque_node);
        }
    }
}

//**************************barrier part********************
void init_barriers(void){
    for(int i = 0; i < BARRIER_NUM; i++){
        global_barrier[i].current_barrier_num = 0;
        global_barrier[i].target_barrier_num = 0;
        global_barrier[i].barrier_key = 0;
        Initialize_QueueNode(&(global_barrier[i].barrier_wait_list));
    }
}

int do_barrier_init(int key, int goal){
    for(int i = 0; i < BARRIER_NUM; i++){
        if(global_barrier[i].barrier_key == 0){
            global_barrier[i].barrier_key = key;
            global_barrier[i].current_barrier_num = 0;
            global_barrier[i].target_barrier_num = goal;
            list_head *barrier_wait_head = &(global_barrier[i].barrier_wait_list);
            barrier_wait_head -> next = barrier_wait_head;
            return i;
        }
    }
    return -1;
}

void do_barrier_wait(int bar_idx){
    global_barrier[bar_idx].current_barrier_num += 1;
    if(global_barrier[bar_idx].current_barrier_num >= global_barrier[bar_idx].target_barrier_num){
        list_head *target_head = &(global_barrier[bar_idx].barrier_wait_list);
        while(target_head -> next != target_head){
            list_head *deque_node = Deque_FromHead(&(global_barrier[bar_idx].barrier_wait_list));
            do_unblock(deque_node);
        }
        global_barrier[bar_idx].current_barrier_num = 0;
    }
    else{
        do_block(&(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &(global_barrier[bar_idx].barrier_wait_list));
    }
}

void do_barrier_destroy(int bar_idx){
    global_barrier[bar_idx].current_barrier_num = 0;
    global_barrier[bar_idx].target_barrier_num = 0;
    global_barrier[bar_idx].barrier_key = 0;

    list_head *target_head = &(global_barrier[bar_idx].barrier_wait_list);
    while(target_head -> next != target_head){
        list_head *deque_node = Deque_FromHead(&(global_barrier[bar_idx].barrier_wait_list));
        do_unblock(deque_node);
    }
}

//****************semaphore part **************************
void init_semaphores(void){
    for(int i = 0; i < SEMAPHORE_NUM; i++){
        global_semaphore_resource[i].sem_number = 0;
        global_semaphore_resource[i].sem_key = 0;
        global_semaphore_resource[i].occupied_or_not = 0;   // unoccupied!
        Initialize_QueueNode(&(global_semaphore_resource[i].sema_wait_list));
    }
}

int do_semaphore_init(int key, int init){
    for(int i = 0; i < SEMAPHORE_NUM; i++){
        if(global_semaphore_resource[i].occupied_or_not == 0){
            global_semaphore_resource[i].sem_number = init;
            global_semaphore_resource[i].sem_key = key;
            global_semaphore_resource[i].occupied_or_not = 1;
            return i;
        }
    }
    return -1;
}

// consumer --
void do_semaphore_down(int sema_idx){
    global_semaphore_resource[sema_idx].sem_number -= 1;
    if(global_semaphore_resource[sema_idx].sem_number < 0){
        do_block(&(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &(global_semaphore_resource[sema_idx].sema_wait_list));
    }
}

// producer ++
void do_semaphore_up(int sema_idx){
    global_semaphore_resource[sema_idx].sem_number += 1;
    if(global_semaphore_resource[sema_idx].sem_number <= 0){
        list_head *deque_node = Deque_FromHead(&(global_semaphore_resource[sema_idx].sema_wait_list));
        if(deque_node != NULL){
            do_unblock(deque_node);
        }
    }
}

void do_semaphore_destroy(int sema_idx){
    global_semaphore_resource[sema_idx].sem_number = 0;
    global_semaphore_resource[sema_idx].sem_key = 0;
    global_semaphore_resource[sema_idx].occupied_or_not = 0;
    list_head *target_head = &(global_semaphore_resource[sema_idx].sema_wait_list);
    while(target_head -> next != target_head){
        list_head *deque_node = Deque_FromHead(&(global_semaphore_resource[sema_idx].sema_wait_list));
        do_unblock(deque_node);
    }
}

//*************************************Condition part *************************************************
void init_conditions(void){
    for(int i = 0; i < CONDITION_NUM; i++){
        global_condition[i].numWaiting = 0;
        Initialize_QueueNode(&(global_condition[i].condition_wait_list));
        global_condition[i].condition_key = -1;
    }
}

int do_condition_init(int key){
    for(int i = 0; i < CONDITION_NUM; i++){
        if(global_condition[i].condition_key == -1){
            global_condition[i].numWaiting = 0;
            global_condition[i].condition_key = key;
            return i;
        }
    }
    return -1;
}

void do_condition_wait(int cond_idx, int mutex_idx){
    global_condition[cond_idx].numWaiting++;    // another process to sleep
    // rewrite do_block start
    list_node_t *pcb_node = &(global_cpu[get_current_cpu_id()].cpu_current_running -> list);
    if(FindNode_InQueue(&ready_queue, pcb_node) == 1){
        DequeNode_AccordList(&ready_queue, pcb_node);
    }
    pcb_t *get_block_pcb = GetPcb_FromList(pcb_node);
    get_block_pcb -> status = TASK_BLOCKED;
    Enque_FromTail( &(global_condition[cond_idx].condition_wait_list), pcb_node);
    // rewrite do_block end
    do_mutex_lock_release(mutex_idx);
    do_scheduler();
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx){
    if(global_condition[cond_idx].numWaiting > 0){
       list_head *deque_node = Deque_FromHead(&(global_condition[cond_idx].condition_wait_list));
       if(deque_node != NULL){
            do_unblock(deque_node);
       }
       global_condition[cond_idx].numWaiting--;
    }
}

void do_condition_broadcast(int cond_idx){
    while(global_condition[cond_idx].numWaiting > 0){
        list_head *deque_node = Deque_FromHead(&(global_condition[cond_idx].condition_wait_list));
        if(deque_node != NULL){
            do_unblock(deque_node);
        }
        global_condition[cond_idx].numWaiting--;
    }
}

void do_condition_destroy(int cond_idx){
    do_condition_broadcast(cond_idx);
    global_condition[cond_idx].numWaiting = 0;
    global_condition[cond_idx].condition_key = -1;
}

//*****************************mailbox part************************************************************
void init_mbox(){
    for(int i = 0; i < MBOX_NUM; i++){
        global_mailbox[i].mailbox_name[0] = '\0';       // Note!: this kind of initialization, strcmp(global_mailbox[i].mailbox_name, "") == 0
        global_mailbox[i].mailbox_Buffer[0] = '\0';
        global_mailbox[i].valid_count = 0;
        global_mailbox[i].visited = 0;
        Initialize_QueueNode(&(global_mailbox[i].mailbox_send_wait_list));
        Initialize_QueueNode(&(global_mailbox[i].mailbox_recv_wait_list));
    }
}

// return a mailbox_name according to name
int do_mbox_open(char *name){
    for(int i = 0; i < MBOX_NUM; i++){
        if(strcmp(global_mailbox[i].mailbox_name, name) == 0){
            global_mailbox[i].visited++;
            return i;
        }
    }

    for(int i = 0; i < MBOX_NUM; i++){
        if(strcmp(global_mailbox[i].mailbox_name, "") == 0){
            strcpy(global_mailbox[i].mailbox_name, name);
            global_mailbox[i].mailbox_Buffer[0] = '\0';
            global_mailbox[i].valid_count = 0;
            global_mailbox[i].visited = 1;
            return i;
        }
    }
    return -1;
}

void do_mbox_close(int mbox_idx){
    global_mailbox[mbox_idx].mailbox_name[0] = '\0';
    global_mailbox[mbox_idx].mailbox_Buffer[0] = '\0';
    global_mailbox[mbox_idx].valid_count = 0;
    global_mailbox[mbox_idx].visited = 0;
    list_head *target_send_head = &(global_mailbox[mbox_idx].mailbox_send_wait_list);
    while(target_send_head -> next != target_send_head){
        list_head *deque_send_node = Deque_FromHead(&(global_mailbox[mbox_idx].mailbox_send_wait_list));
        do_unblock(deque_send_node);
    }
    list_head *target_recv_head = &(global_mailbox[mbox_idx].mailbox_recv_wait_list);
    while(target_recv_head -> next != target_recv_head){
        list_head *deque_recv_node = Deque_FromHead(&(global_mailbox[mbox_idx].mailbox_recv_wait_list));
        do_unblock(deque_recv_node);
    }
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length){
    while(1){
        printl("\n\n[Send]: current_running [%d]: %s mailbox[%d]\n", global_cpu[get_current_cpu_id()].cpu_current_running -> pid, global_cpu[get_current_cpu_id()].cpu_current_running -> name, mbox_idx);
        printl("[Send]: valid_count_before: %d\n", global_mailbox[mbox_idx].valid_count);

        int start_length = global_mailbox[mbox_idx].valid_count;
        int blocked_times = 0;
        if(start_length + msg_length > MAX_MBOX_LENGTH){
            // block_part
            printl("[Send]: Option-1, blocked!\n");
            blocked_times++;
            do_block( &(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &(global_mailbox[mbox_idx].mailbox_send_wait_list));
            // block_part
        }
        else{
            char *msg_ptr = (char *)msg;
            for(int i = 0; i < msg_length; i++){
                global_mailbox[mbox_idx].mailbox_Buffer[start_length + i] = msg_ptr[i];
            }

            global_mailbox[mbox_idx].valid_count += msg_length;
            // Wakeup --- receive_queue

            if( (&(global_mailbox[mbox_idx].mailbox_recv_wait_list )) -> next != &(global_mailbox[mbox_idx].mailbox_recv_wait_list)){
                printl("[Send]: recv_wait_list Release before\n");
                PrintPcb_FromList( &(global_mailbox[mbox_idx].mailbox_recv_wait_list));
            }
            list_head *target_head = &(global_mailbox[mbox_idx].mailbox_recv_wait_list);
            while(target_head -> next != target_head){
                list_head *deque_node = Deque_FromHead(&(global_mailbox[mbox_idx].mailbox_recv_wait_list));
                do_unblock(deque_node);
            }

            //**************debug**************
            printl("[Send]: Option-2, not-blocked!\n");
            printl("[Send]: ready_queue \n");
            PrintPcb_FromList(&ready_queue);
            printl("[Send]: send_wait_list \n");
            PrintPcb_FromList( &(global_mailbox[mbox_idx].mailbox_send_wait_list));
            //*************debug**************
            return blocked_times;
        }
    }
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length){
    while(1){
        printl("\n\n[Receive]: current_running [%d]: %s mailbox[%d]\n", global_cpu[get_current_cpu_id()].cpu_current_running -> pid, global_cpu[get_current_cpu_id()].cpu_current_running -> name, mbox_idx);
        printl("[Receive]: valid_count_before: %d\n", global_mailbox[mbox_idx].valid_count);

        int total_number = global_mailbox[mbox_idx].valid_count;
        int blocked_times = 0;
        if(total_number < msg_length){  // need to block
        // block_part
            printl("[Receive]: Option-1, blocked!\n");
            blocked_times++;
            do_block(&(global_cpu[get_current_cpu_id()].cpu_current_running -> list), &(global_mailbox[mbox_idx].mailbox_recv_wait_list));
        // block_part
        }
        else{
            char *msg_ptr = (char *)msg;
            for(int i = 0; i < msg_length; i++){
                msg_ptr[i] = global_mailbox[mbox_idx].mailbox_Buffer[i];
            }
            // copy the rest to the front
            // [ msg_ptr ][valid_number]
            // move proceduler
            int count = 0;
            for(int k = msg_length; k < total_number; k++){
                global_mailbox[mbox_idx].mailbox_Buffer[count++] = global_mailbox[mbox_idx].mailbox_Buffer[k];
            }
            for(int m = count; m < total_number; m++){
                global_mailbox[mbox_idx].mailbox_Buffer[count++] = '\0';
            }
            global_mailbox[mbox_idx].valid_count -= msg_length;
            // Wakeup ---- SendQueue

            if( (&(global_mailbox[mbox_idx].mailbox_send_wait_list )) -> next != &(global_mailbox[mbox_idx].mailbox_send_wait_list)){
                printl("[Receive]: send_wait_list Release before\n");
                PrintPcb_FromList( &(global_mailbox[mbox_idx].mailbox_send_wait_list));
            }

            list_head *target_head = &(global_mailbox[mbox_idx].mailbox_send_wait_list);
            while(target_head -> next != target_head){
                list_head *deque_node = Deque_FromHead(&(global_mailbox[mbox_idx].mailbox_send_wait_list));
                do_unblock(deque_node);
            }

            //**************debug**************
            printl("[Receive]: Option-2, not-blocked!\n");
            printl("[Receive]: ready_queue \n");
            PrintPcb_FromList(&ready_queue);
            printl("[Receive]: recv_wait_list \n");
            PrintPcb_FromList( &(global_mailbox[mbox_idx].mailbox_recv_wait_list));
            //*************debug**************
            return blocked_times;
        }
    }
}

// implement cpu related cpu
void init_global_cpu(){
    int cpuid = get_current_cpu_id();
    global_cpu[cpuid].cpu_id = cpuid;
    global_cpu[cpuid].cpu_current_running = &pcb[0];
}

struct cpu *get_current_cpu(){
    return &(global_cpu[get_current_cpu_id()]);
}


//*******************************************************************************************
// implement Large_Kernel_lock
void kernel_spin_lock_init(kernel_spin_lock *lock){
    lock -> spin_lock_state = 0;
    lock -> cpu_own_this_lock = NULL;
}


void kernel_spin_lock_acquire(){
    kernel_spin_lock *lock = &Large_Kernel_Lock;
    struct cpu *mycpu = get_current_cpu();
    if(lock -> spin_lock_state == 1 && lock -> cpu_own_this_lock == mycpu){
        return;
    }
    while(atomic_swap_d(1, &(lock -> spin_lock_state)) != 0){
        ;  // spin
    }
    lock -> cpu_own_this_lock = mycpu;
}

void kernel_spin_lock_release(){
    kernel_spin_lock *lock = &Large_Kernel_Lock;
    if(lock -> cpu_own_this_lock != get_current_cpu() || lock -> spin_lock_state == 0){
        return ; // this cpu do not hold the lock, can not release
    }
    while(atomic_swap_d(0, &(lock -> spin_lock_state)) != 1){
        ;
    }
    lock -> cpu_own_this_lock = NULL;
}