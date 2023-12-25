/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
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

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <printk.h>     // newly added for print queue

#define NUM_MAX_TASK 16
#define EI_NIDENT  16
#define LIST_IN_PCB_OFFSET 32  //  (unsigned long) (&((pcb_t *)0)-> list);

/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
    reg_t regs_pointer;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
} task_status_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;                // 0
    reg_t user_sp;                  // 8
    ptr_t kernel_stack_base;        // 16
    ptr_t user_stack_base;          // 24

    /* previous, next pointer */
    list_node_t list;               // 32
    list_head wait_list;            // 48

    /* process id */
    pid_t pid;                      // 64

    /* BLOCK | READY | RUNNING */
    task_status_t status;           // 68

    /* cursor position */
    int cursor_x;                   // 72
    int cursor_y;                   // 76

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;           // 80
    switchto_context_t pcb_switchto_context;    // 88
    regs_context_t pcb_user_regs_context;       // 200
    char name[EI_NIDENT];                       // 496
} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* lock_queue can be implemented on lock.h*/

/* current running task PCB */
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t pid0_pcb;
extern const ptr_t pid0_stack;

extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);

/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
/************************************************************/

// use list to find the whole pcb
static inline pcb_t *GetPcb_FromList(list_head *node){
   unsigned long list_offset = (unsigned long) (&((pcb_t *)0)-> list);
   pcb_t *return_pcb = NULL;
   return_pcb = (pcb_t *) ((char *)(node) - list_offset);
   return return_pcb;
}

// use wait_queue to find the whole pcb
static inline pcb_t *GetPcb_FromWaitList(list_head *node){
    unsigned long list_offset = (unsigned long) (&((pcb_t *)0)-> wait_list);
    pcb_t *return_pcb = NULL;
    return_pcb = (pcb_t *) ((char *)(node) - list_offset);
    return return_pcb;
}

static inline void PrintPcb_FromList(list_head *head){
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
#endif