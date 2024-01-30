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
#define BOOT_LOADER_ADDRESS 0xffffffc050200000
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

    // P3-part1
    syscall[SYSCALL_READCH]         = (long (*)())bios_getchar;
    syscall[SYSCALL_PS]             = (long (*)())do_process_show;
    syscall[SYSCALL_CLEAR]          = (long (*)())screen_clear;

    // P3-part1
    syscall[SYSCALL_EXEC]           = (long (*)())do_exec;
    syscall[SYSCALL_WAITPID]        = (long (*)())do_waitpid;
    syscall[SYSCALL_EXIT]           = (long (*)())do_exit;
    syscall[SYSCALL_GETPID]         = (long (*)())do_getpid;
    syscall[SYSCALL_KILL]           = (long (*)())do_kill;

    // P3-part2-barrier
    syscall[SYSCALL_BARR_INIT]      = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]      = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]   = (long (*)())do_barrier_destroy;

    // P3-part2-semaphore-producer-consumer
    syscall[SYSCALL_SEMA_INIT]     = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]       = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]     = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]  = (long (*)())do_semaphore_destroy;

    // P3-part2-condition
    syscall[SYSCALL_COND_INIT]    = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT]    = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]  = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]  = (long (*)())do_condition_destroy;

    // P3-part2-mailbox
    syscall[SYSCALL_MBOX_OPEN]     = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]    = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]     = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]     = (long (*)())do_mbox_recv;

    // P3-part4-taskset
    syscall[SYSCALL_TASKSET]       = (long (*)())do_taskset;
}

/************************************************************/
static void init_time(void){
    // assume sstatus.sie = 1, and sie.stie = 1
    time_elapsed = 0;
    clock_trigger_next_interrupt();
}

static void init_large_kernel(void){
    kernel_spin_lock_init(&Large_Kernel_Lock);
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    int cpuid = get_current_cpu_id();
    if(cpuid == 0){
        init_clean_boot_address_map(); 

        init_global_cpu();

        init_jmptab();
        // Init task information (〃'▽'〃)
        init_task_info();
        // Init Process Control Blocks |•'-'•) ✧
        // only used for printk
        init_pcb_loop();
        printk("> [INIT] Pid0 initialization succeeded.\n");
        
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

        init_large_kernel();
        printk("> [INIT] LARGE_KERNEL_LOCK initialization succeeded. \n");
        

        // Newly added, print cpu_id
        printk("> [Current cpu_id]: %d\n", cpuid);
        
        // Init barrier (newly added! o.0)
        init_barriers();
        printk("> [INIT] Barrier initialization succeeded.\n");

        // Init semaphore (newly added! o.0)
        init_semaphores();
        printk("> [INIT] Semaphores initialization succeeded.\n");

        // Init condition (newly added! o.0)
        init_conditions();
        printk("> [INIT] Condition initialization succeeded.\n");

        // Init mailbox (newly added! o.0)
        init_mbox();
        printk("> [INIT] Mailbox initialization succeeded.\n");

        // send_ipi(NULL);

        kernel_spin_lock_acquire();

        init_time();
        kernel_spin_lock_release();
        while(1){
            enable_preempt();
            asm volatile("wfi");
        }
    }
    else{
        kernel_spin_lock_acquire();
        init_global_cpu();      // init global_cpu struct
        // Newly added, print cpu_id
        printk("> [Current cpu_id]: %d\n", cpuid);
        
        setup_exception();
        printk("> [INIT-%d] Interrupt processing initialization succeeded.\n", cpuid);
        
        init_time();
        kernel_spin_lock_release();

        while(1){
            enable_preempt();
            asm volatile("wfi");
        }
    }
    return 0;
}