/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Thread Lock
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_LOCK_H_
#define INCLUDE_LOCK_H_

#include <os/list.h>
#include <os/sched.h>   // newly added

#define LOCK_NUM 16

typedef enum {
    UNLOCKED,
    LOCKED,
} lock_status_t;

typedef struct spin_lock
{
    volatile lock_status_t status;
} spin_lock_t;

typedef struct mutex_lock
{
    spin_lock_t lock;
    list_head block_queue;
    int key;
    pcb_t *lock_owner; // newly added, only for debug!
} mutex_lock_t;

mutex_lock_t mlocks[LOCK_NUM]; // change from lock.c to lock.h
void init_locks(void);

void spin_lock_init(spin_lock_t *lock);
int spin_lock_try_acquire(spin_lock_t *lock);
void spin_lock_acquire(spin_lock_t *lock);
void spin_lock_release(spin_lock_t *lock);

int do_mutex_lock_init(int key);
void do_mutex_lock_acquire(int mlock_idx);
void do_mutex_lock_release(int mlock_idx);

/************************************************************/
typedef struct barrier
{
    // TODO [P3-TASK2 barrier]
    int current_barrier_num;        // initialize as 0
    int target_barrier_num;         // target
    int barrier_key;                // key_identify
    list_head barrier_wait_list;    // wait_queue 
} barrier_t;

#define BARRIER_NUM 16

barrier_t global_barrier[BARRIER_NUM]; // newly added!

void init_barriers(void);
int do_barrier_init(int key, int goal);
void do_barrier_wait(int bar_idx);
void do_barrier_destroy(int bar_idx);

typedef struct condition
{
    // TODO [P3-TASK2 condition]
    int numWaiting;         // num of processes wait on this condition
    list_head condition_wait_list;
    int condition_key;      // -1 means unoccpuied
} condition_t;

#define CONDITION_NUM 16

condition_t global_condition[CONDITION_NUM];

void init_conditions(void);
int do_condition_init(int key);
void do_condition_wait(int cond_idx, int mutex_idx);
void do_condition_signal(int cond_idx);
void do_condition_broadcast(int cond_idx);
void do_condition_destroy(int cond_idx);

typedef struct semaphore
{
    // TODO [P3-TASK2 semaphore]
    int sem_number;             // semaphore_total
    int sem_key;                // origin set to 0
    int occupied_or_not;        // origin set to 0 [0]: free [1]: occupied
    list_head sema_wait_list;   // semaphore_waitlist
} semaphore_t;

#define SEMAPHORE_NUM 16

semaphore_t global_semaphore_resource[SEMAPHORE_NUM];       // semaphore_base

void init_semaphores(void);
int do_semaphore_init(int key, int init);
void do_semaphore_up(int sema_idx);
void do_semaphore_down(int sema_idx);
void do_semaphore_destroy(int sema_idx);


#define MAX_MBOX_LENGTH (64)

#define MAILBOX_NAME_LENGTH (64)

typedef struct mailbox
{
    // TODO [P3-TASK2 mailbox]
    char mailbox_name[MAILBOX_NAME_LENGTH];      // name
    char mailbox_Buffer[MAX_MBOX_LENGTH];       // context that mailbox store
    int valid_count;                            // valid character number
    int visited;                                // visit times
    list_head mailbox_send_wait_list;           // full, send_block
    list_head mailbox_recv_wait_list;           // empty, recv_block
} mailbox_t;

#define MBOX_NUM 16

mailbox_t global_mailbox[MBOX_NUM];

void init_mbox();
int do_mbox_open(char *name);
void do_mbox_close(int mbox_idx);
int do_mbox_send(int mbox_idx, void * msg, int msg_length);
int do_mbox_recv(int mbox_idx, void * msg, int msg_length);

/************************************************************/
typedef struct cpu{
    int cpu_id;                 
    pcb_t *cpu_current_running;  // Every cpu should have it's own current_running
} cpu;

extern cpu global_cpu[2];

void init_global_cpu();             // fill global_cpu
struct cpu *get_current_cpu();      // return pointer

// Newly added, implement large_kernel_spin_lock
typedef struct kernel_spin_lock{
    volatile int spin_lock_state;   // 0: unlocked, 1: locked
    struct cpu *cpu_own_this_lock;  // used for debug
} kernel_spin_lock;

kernel_spin_lock Large_Kernel_Lock;

void kernel_spin_lock_init(kernel_spin_lock *lock);
void kernel_spin_lock_acquire();
void kernel_spin_lock_release();

#endif