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
    bios_sd_read(tasks, 2, 1);
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    *(ptr_t*)(pt_switchto) = entry_point;       // ra
    *(ptr_t*)(pt_switchto + 8) = user_stack;    // sp
    
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    Initialize_QueueNode(&ready_queue);
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    for(int i = 0; i <= task_num; i++){
        pcb[i].kernel_sp = allocKernelPage(1);
        pcb[i].user_sp = allocUserPage(1);

        Initialize_QueueNode(&pcb[i].list);

        pcb[i].pid = i;

        if(strcmp(tasks[i].taskname, "print1") == 0 || strcmp(tasks[i].taskname, "print2") == 0 || strcmp(tasks[i].taskname, "fly") == 0){
            // task1 - first load the three process
            pcb[i].status = TASK_READY;
        }

        pcb[i].pcb_switchto_context.regs[0] = TASK_MEM_BASE + (i - 1) * TASK_SIZE;
        if(strcmp(tasks[i].taskname, "main") == 0){
            pcb[i].pcb_switchto_context.regs[0] = BOOT_LOADER_ADDRESS + (TASK_SIZE >> 4);
        }
        strcpy(pcb[i].name, tasks[i].taskname);
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, pcb[i].pcb_switchto_context.regs[0], &pcb[i]);
    }

    for(int i = 0; i <= task_num; i++){
        if(pcb[i].status == TASK_READY){
            Enque_FromTail(&ready_queue, &pcb[i].list);
        }
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pcb[0];
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
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
    //printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    //printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    //printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    //printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    //printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    


    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    do_scheduler();


    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    // while (1)
    // {
    //     // If you do non-preemptive scheduling, it's used to surrender control
    //     do_scheduler();

    //     // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
    //     // enable_preempt();
    //     // asm volatile("wfi");
    // }

    return 0;
}