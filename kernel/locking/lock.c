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
            mlocks[mlock_idx].lock_owner = current_running;
            spin_lock_acquire(&(mlocks[mlock_idx].lock));
            break;
        }
        else{
            do_block(&(current_running -> list), &(mlocks[mlock_idx].block_queue));
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
        do_block(&(current_running -> list), &(global_barrier[bar_idx].barrier_wait_list));
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
        do_block(&(current_running -> list), &(global_semaphore_resource[sema_idx].sema_wait_list));
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
    list_node_t *pcb_node = &(current_running -> list);
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
        Initialize_QueueNode(&(global_mailbox[i].mailbox_wait_list));
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
        if(global_mailbox[i].mailbox_name[0] == '\0'){
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
    list_head *target_head = &(global_mailbox[mbox_idx].mailbox_wait_list);
    while(target_head -> next != target_head){
        list_head *deque_node = Deque_FromHead(&(global_mailbox[mbox_idx].mailbox_wait_list));
        do_unblock(deque_node);
    }
}