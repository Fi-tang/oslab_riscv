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

//*********************** BoundedBuffer part *****************************
void init_BoundedBuffer(void){
    for(int i = 0; i < BOUNDEDBUFFER_NUM; i++){
        global_bounded_buffer[i].count = 0;
        global_bounded_buffer[i].mutex_sem_idx = -1;
        global_bounded_buffer[i].fullBuffer_sem_idx = -1;
        global_bounded_buffer[i].emptyBuffer_sem_idx = -1;
    }
}

int do_BoundedBuffer_init(int key, int init){
    for(int i = 0; i < BOUNDEDBUFFER_NUM; i++){
        if(global_bounded_buffer[i].mutex_sem_idx == -1 &&  global_bounded_buffer[i].fullBuffer_sem_idx == -1 && global_bounded_buffer[i].emptyBuffer_sem_idx == -1){
            global_bounded_buffer[i].mutex_sem_idx = do_semaphore_init(key, 1);
            global_bounded_buffer[i].BoundedBuffer_mutex = global_semaphore_resource[global_bounded_buffer[i].mutex_sem_idx];
            global_bounded_buffer[i].fullBuffer_sem_idx = do_semaphore_init(key, 0);
            global_bounded_buffer[i].BoundedBuffer_fullBuffers = global_semaphore_resource[global_bounded_buffer[i].fullBuffer_sem_idx];
            global_bounded_buffer[i].emptyBuffer_sem_idx = do_semaphore_init(key, init);
            global_bounded_buffer[i].BoundedBuffer_emptyBuffers = global_semaphore_resource[global_bounded_buffer[i].emptyBuffer_sem_idx];
            return i;
        }
    }
    return -1;
}

// producer ++
void do_BoundedBuffer_up(int boundedbuffer_idx){
    // emptyBuffer -> down
    do_semaphore_down(global_bounded_buffer[boundedbuffer_idx].emptyBuffer_sem_idx);
    // lock -> down
    do_semaphore_down(global_bounded_buffer[boundedbuffer_idx].mutex_sem_idx);
    // add an element 
    global_bounded_buffer[boundedbuffer_idx].count++;
    // lock -> up
    do_semaphore_up(global_bounded_buffer[boundedbuffer_idx].mutex_sem_idx);
    // fullBuffer -> up
    do_semaphore_up(global_bounded_buffer[boundedbuffer_idx].fullBuffer_sem_idx);
}

// consumer --
void do_BoundedBuffer_down(int boundedbuffer_idx){
    // fullBuffer -> down
    do_semaphore_down(global_bounded_buffer[boundedbuffer_idx].fullBuffer_sem_idx);
    // lock -> down
    do_semaphore_down(global_bounded_buffer[boundedbuffer_idx].mutex_sem_idx);
    // remove an element
    global_bounded_buffer[boundedbuffer_idx].count--;
    // lock -> up
    do_semaphore_up(global_bounded_buffer[boundedbuffer_idx].mutex_sem_idx);
    // emptyBuffer -> up
    do_semaphore_up(global_bounded_buffer[boundedbuffer_idx].emptyBuffer_sem_idx);
}

void do_BoundedBuffer_destroy(int boundedbuffer_idx){
    do_semaphore_destroy(global_bounded_buffer[boundedbuffer_idx].mutex_sem_idx);
    do_semaphore_destroy(global_bounded_buffer[boundedbuffer_idx].fullBuffer_sem_idx);
    do_semaphore_destroy(global_bounded_buffer[boundedbuffer_idx].emptyBuffer_sem_idx);
    global_bounded_buffer[boundedbuffer_idx].count = 0;
    global_bounded_buffer[boundedbuffer_idx].mutex_sem_idx = -1;
    global_bounded_buffer[boundedbuffer_idx].fullBuffer_sem_idx = -1;
    global_bounded_buffer[boundedbuffer_idx].emptyBuffer_sem_idx = -1;
}