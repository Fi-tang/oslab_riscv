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
    user_regs_context -> sepc = entry_point;        // restore context, first happen because in ret_from_exception:[sepc + 4]
    user_regs_context -> sbadaddr = 0;
    user_regs_context -> scause = SCAUSE_IRQ_FLAG + IRQC_S_TIMER;
    user_regs_context -> regs_pointer = &(pcb -> pcb_user_regs_context);
}

static void init_shell(void){
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    Initialize_QueueNode(&ready_queue); 
    for(int i = 0; i <= task_num; i++){
        if(strcmp(tasks[i].taskname, "main") == 0){
            pcb[0].kernel_sp = allocKernelPage(1);
            pcb[0].user_sp = allocUserPage(1);

            pcb[0].kernel_stack_base = pcb[0].kernel_sp;
            pcb[0].user_stack_base = pcb[0].user_sp;

            pcb[0].cursor_x = 0;
            pcb[0].cursor_y = 0;
            Initialize_QueueNode(&pcb[0].list);
            Initialize_QueueNode(&pcb[0].wait_list);

            pcb[0].pid = 0;
            strcpy(pcb[0].name, tasks[i].taskname);
            init_pcb_regs(&pcb[0].pcb_switchto_context, &pcb[0].pcb_user_regs_context, &pcb[0], BOOT_LOADER_ADDRESS + (TASK_SIZE >> 4));
        }
        else if(strcmp(tasks[i].taskname, "shell") == 0){
            pcb[1].kernel_sp = allocKernelPage(1);
            pcb[1].user_sp = allocUserPage(1);
            
            pcb[1].kernel_stack_base = pcb[1].kernel_sp;
            pcb[1].user_stack_base = pcb[1].user_sp;

            pcb[1].cursor_x = 1;
            pcb[1].cursor_y = 1;
           
            Initialize_QueueNode(&pcb[1].list);
            Initialize_QueueNode(&pcb[1].wait_list);

            pcb[1].pid = 1;
            strcpy(pcb[1].name, tasks[1].taskname);
            long current_task_entry_address = load_task_img_by_name(task_num, pcb[1].name);
            pcb[1].status = TASK_READY;
            init_pcb_regs(&pcb[1].pcb_switchto_context, &pcb[1].pcb_user_regs_context, &pcb[1], current_task_entry_address);
            
            Enque_FromTail(&ready_queue, &pcb[1].list);
        }
    }

    for(int i = 2; i < NUM_MAX_TASK; i++){
        pcb[i].pid = i;
        pcb[i].status = TASK_EXITED;
    }

    current_running = &pcb[0];
    asm volatile("mv tp, %0" ::"r"(current_running));
}


pid_t do_exec(char *name, int argc, char *argv[]){
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    for(int i = 0; i < NUM_MAX_TASK; i++){
        if(pcb[i].status == TASK_EXITED){
            pcb[i].kernel_sp = allocKernelPage(1);
            pcb[i].user_sp = allocUserPage(1);

            pcb[i].kernel_stack_base = pcb[i].kernel_sp;
            pcb[i].user_stack_base = pcb[i].user_sp;

            pcb[i].cursor_x = i;
            pcb[i].cursor_y = i;
            Initialize_QueueNode(&pcb[i].list);
            Initialize_QueueNode(&pcb[i].wait_list);

            strcpy(pcb[i].name, name);
            long current_task_entry_address = load_task_img_by_name(task_num, name);
            pcb[i].status = TASK_READY;

            init_pcb_regs(&pcb[i].pcb_switchto_context, &pcb[i].pcb_user_regs_context, &pcb[i], current_task_entry_address);
            
            // write argc and argv to a0 and a1 register!
            pcb[i].pcb_user_regs_context.regs[10] = argc;
            pcb[i].pcb_user_regs_context.regs[11] = argv;


            Enque_FromTail(&ready_queue, &pcb[i].list);
            return i;
        }
    }
    return 0;
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
}
/************************************************************/
static void init_time(void){
    // assume sstatus.sie = 1, and sie.stie = 1
    asm volatile(
        "addi a0, zero, 0x20\n"
        "csrw sie, a0\n");
    time_elapsed = 0;
    clock_trigger_next_interrupt();
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    // only used for printk
    init_shell();
    printk("> [INIT] Shell initialization succeeded.\n");

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
    // Init time interrupt 
    init_time();
    printk("> [INIT] Time interrupt initialization succeed.\n");

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}