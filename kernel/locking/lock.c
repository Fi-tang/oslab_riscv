#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <printk.h> // newly added 

mutex_lock_t mlocks[LOCK_NUM];

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
    // printk("\n[LOCK_ACQUIRE]: %s trying to acquire lock\n", current_running -> name);
    if(spin_lock_try_acquire(&(mlocks[mlock_idx].lock)) == 1){
        // success
        mlocks[mlock_idx].lock_owner = current_running;
        spin_lock_acquire(&(mlocks[mlock_idx].lock)); 
    }
    else{
        // failed!
        do_block(&(current_running -> list), &(mlocks[mlock_idx].block_queue));
        do_mutex_lock_acquire(mlock_idx);
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