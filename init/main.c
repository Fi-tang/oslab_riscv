#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
// !!![warning] have no reference for <os/list.h>
#include <os/list.h>

#define VERSION_BUF 50
#define BOOT_LOADER_ADDRESS 0x50200000
#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];


static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.
    jmptab[REFLUSH]        = (long (*)())screen_reflush;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    bios_sd_read(&tasks, 2, 1);
}

/************************************************************/
static void init_pcb_regs(switchto_context_t *kernel_switchto_context, regs_context_t *user_regs_context, pcb_t *pcb, ptr_t entry_point){
    // ********************* kernel ***************************//
    kernel_switchto_context -> regs[0] = ret_from_exception; // ra
    kernel_switchto_context -> regs[1] = pcb -> kernel_sp; // sp

    //********************** user ****************************//
    user_regs_context -> regs[1] = entry_point;
    user_regs_context -> regs[2] = pcb -> user_sp;
    user_regs_context -> regs[4] = pcb;

    user_regs_context -> sstatus = SR_SPIE;
    user_regs_context -> sepc = entry_point - 4;        // restore context, first happen because in ret_from_exception:[sepc + 4]
    user_regs_context -> sbadaddr = 0;
    user_regs_context -> scause = 0;
    user_regs_context -> regs_pointer = &(pcb -> pcb_user_regs_context);
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    Initialize_QueueNode(&ready_queue);
    Initialize_QueueNode(&sleep_queue);
    for(int i = 0; i <= task_num; i++){
        pcb[i].kernel_sp = allocKernelPage(1);  // kernel_sp
        pcb[i].user_sp = allocUserPage(1);      // user_sp;

        pcb[i].cursor_x = i;
        pcb[i].cursor_y = i;
        Initialize_QueueNode(&pcb[i].list);
        
        pcb[i].pid = i;
        strcpy(pcb[i].name, tasks[i].taskname);

        if(strcmp(tasks[i].taskname, "main") == 0){
            init_pcb_regs(&pcb[i].pcb_switchto_context, &pcb[i].pcb_user_regs_context, &pcb[i], BOOT_LOADER_ADDRESS + (TASK_SIZE >> 4));
        }
        else{
            long current_task_entry_address = load_task_img_by_name(task_num, pcb[i].name);
            if(strcmp(pcb[i].name, "print1") == 0 || strcmp(pcb[i].name, "print2") == 0 ){
                pcb[i].status = TASK_READY;
            }
            if(strcmp(pcb[i].name, "lock1") == 0 || strcmp(pcb[i].name, "lock2") == 0 || strcmp(pcb[i].name, "fly") == 0){
                pcb[i].status = TASK_READY;
            }
            if(strcmp(pcb[i].name, "sleep") == 0 || strcmp(pcb[i].name, "timer") == 0){
                pcb[i].status = TASK_READY;
            }
            init_pcb_regs(&pcb[i].pcb_switchto_context, &pcb[i].pcb_user_regs_context, &pcb[i], current_task_entry_address);
        }  
    }

    for(int i = 0; i <= task_num; i++){
        if(pcb[i].status == TASK_READY){
            Enque_FromTail(&ready_queue, &pcb[i].list);
        }
    }

    current_running = &pcb[0];
    asm volatile("mv tp, %0" : :"r"(current_running));
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]          =  (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]          =  (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE]          =  (long (*)())printk;
    syscall[SYSCALL_CURSOR]         =  (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]        =  (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE]   =  (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]       = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]      = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]       = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]   = (long (*)())do_mutex_lock_release;
}
/************************************************************/

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    // only used for printk
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}