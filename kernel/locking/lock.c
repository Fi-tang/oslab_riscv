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
        global_semaphore[i].sem_num = 0;
        global_semaphore[i].sem_key = 0;
        Initialize_QueueNode(&(global_semaphore[i].sema_wait_list));
    }
}

int do_semaphore_init(int key, int init){
    for(int i = 0; i < SEMAPHORE_NUM; i++){
        if(global_semaphore[i].sem_key == 0){
            global_semaphore[i].sem_num = init;
            global_semaphore[i].sem_key = key;
            return i;
        }
    }
    return -1;
}

// producer, means sem_num++;
void do_semaphore_up(int sema_idx){
    global_semaphore[sema_idx].sem_num++;
    if(global_semaphore[sema_idx].sem_num <= 0){
        list_head *deque_node = Deque_FromHead(&(global_semaphore[sema_idx].sema_wait_list));
        if(deque_node != NULL){
            do_unblock(deque_node);
        }
    }
}

// consumer, means sem_num--;
void do_semaphore_down(int sema_idx){
    global_semaphore[sema_idx].sem_num--;
    if(global_semaphore[sema_idx].sem_num < 0){
        do_block(&(current_running -> list), &(global_semaphore[sema_idx].sema_wait_list));
    }
}

void do_semaphore_destroy(int sema_idx){
    global_semaphore[sema_idx].sem_num = 0;
    global_semaphore[sema_idx].sem_key = 0;

    list_head *target_head = &(global_semaphore[sema_idx].sema_wait_list);
    while(target_head -> next != target_head){
        list_head *deque_node = Deque_FromHead(&(global_semaphore[sema_idx].sema_wait_list));
        do_unblock(deque_node);
    }
}