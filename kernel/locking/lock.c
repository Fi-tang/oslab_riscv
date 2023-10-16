#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <printk.h> // newly added!

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++){
        spin_lock_init(&(mlocks[i].lock));
        mlocks[i].key = 0;
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
    if(lock -> status == UNLOCKED)  return 1;
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    if(spin_lock_try_acquire(lock) == 0){
        // can-not acquire, other holds it
    }
    else{
        lock -> status = LOCKED;
    }
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
        if(mlocks[i].key == 0){
            // means we have not initialized this mutex_lock
            spin_lock_init(&(mlocks[i].lock));
            Initialize_QueueNode(&(mlocks[i].block_queue));
            mlocks[i].key = key;
        }
        return i;
    }
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    if(spin_lock_try_acquire(&(mlocks[mlock_idx].lock)) == 0){
        // failed!
        do_block(&(current_running -> list), &(mlocks[mlock_idx].block_queue));
    }
    else{
        // succeed!
        mlocks[mlock_idx].lock.status = LOCKED;
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    spin_lock_release(&(mlocks[mlock_idx].lock));
    list_head *release_node = &(mlocks[mlock_idx].block_queue);
    while(release_node -> next != release_node){
        list_head *node = Deque_FromHead(release_node);
        do_unblock(node);
    }
    // remain to do wakeup
}