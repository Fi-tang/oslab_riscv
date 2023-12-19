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

static void assign_initial_pcb(char *name, int alloc_index){
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    pcb[alloc_index].kernel_sp = allocKernelPage(1);
    pcb[alloc_index].user_sp = allocUserPage(1);

    pcb[alloc_index].kernel_stack_base = pcb[alloc_index].kernel_sp;
    pcb[alloc_index].user_stack_base = pcb[alloc_index].user_sp;

    pcb[alloc_index].cursor_x = alloc_index;
    pcb[alloc_index].cursor_y = alloc_index;
    Initialize_QueueNode(&pcb[alloc_index].list);
    Initialize_QueueNode(&pcb[alloc_index].wait_list);

    pcb[alloc_index].pid = alloc_index;
    strcpy(pcb[alloc_index].name, name);
    long current_task_entry_address = load_task_img_by_name(task_num, pcb[alloc_index].name);
    pcb[alloc_index].status = TASK_READY;
    init_pcb_regs(&pcb[alloc_index].pcb_switchto_context, &pcb[alloc_index].pcb_user_regs_context, &pcb[0], current_task_entry_address);

    Enque_FromTail(&ready_queue, &pcb[alloc_index].list);
}


static void init_pcb_loop(void){  // cpu [0] always point to pid0, cpu [1] always point to pid1
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    Initialize_QueueNode(&ready_queue); 
    for(int i = 0; i <= task_num; i++){
        if(strcmp(tasks[i].taskname, "pid0") == 0){
            assign_initial_pcb(tasks[i].taskname, 0);
        }
        if(strcmp(tasks[i].taskname, "pid1") == 0){
            assign_initial_pcb(tasks[i].taskname, 1);
        }
    }

    for(int i = 2; i < NUM_MAX_TASK; i++){
        pcb[i].pid = i;
        pcb[i].status = TASK_EXITED;
    }

    current_running = &pid0_pcb;
    asm volatile("mv tp, %0" ::"r"(global_cpu[get_current_cpu_id()].cpu_current_running));
}

void do_writeArgvToMemory(pcb_t *pcb, int argc, char *argv[]){
    if(argc == 0){
        pcb -> pcb_user_regs_context.regs[10] = 0;
        pcb -> pcb_user_regs_context.regs[11] = 0x0;
        return;
    }   
    // need 8 bytes to allocate argv[0] - argv[1] - ... - argv[n]
    reg_t avail_user_stack = pcb -> user_sp;
    int count_mem_usage = 0;
    
    char *filled_with_zero = NULL;
    for(int i = 0; i < 8; i++){
        filled_with_zero = (char *)(pcb -> user_sp - i);
        *filled_with_zero = '\0';
    }
    for(int i = 0; i <= argc; i++){
        avail_user_stack -= 8;
        count_mem_usage += 8;
    }
    
    for(int i = 0; i < argc; i++){
        char **unassigned_location = NULL;
        unassigned_location = (char **)(pcb -> user_sp - (argc - i) * 8);
                        // assign arv[0] --> 'test_barrier'
        int total_number = strlen(argv[i]) + 1;

        avail_user_stack -= total_number;

        char *string_mem = NULL;
        string_mem = (char *)(avail_user_stack + 1);

        strncpy(string_mem, argv[i], strlen(argv[i]));

        *unassigned_location = string_mem;
        count_mem_usage += total_number;
    }

    // write argc and argv to a0 and a1 register!
    pcb -> pcb_user_regs_context.regs[10] = argc;
    pcb -> pcb_user_regs_context.regs[11] = (pcb -> user_sp - (argc * 8));

    pcb -> user_sp = pcb -> user_sp - count_mem_usage;
    while(pcb -> user_sp % 128 != 0){
        pcb -> user_sp -= 1;
    }
}

pid_t do_exec(char *name, int argc, char *argv[]){
    //***********************************************************
    // the argv[] parameter now is all correct!
    //**********************debugging part************************
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

            do_writeArgvToMemory(&pcb[i], argc, argv);

            init_pcb_regs(&pcb[i].pcb_switchto_context, &pcb[i].pcb_user_regs_context, &pcb[i], current_task_entry_address);

            Enque_FromTail(&ready_queue, &pcb[i].list);
            printl("[Do_Exec]: pcb[%d]: %s\n", pcb[i].pid, pcb[i].name);
            PrintPcb_FromList(&ready_queue);
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

static void init_large_kernel(void){
    kernel_spin_lock_init(&Large_Kernel_Lock);
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    int cpuid = get_current_cpu_id();
    if(cpuid == 0){
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

        init_time();
        printk("> [INIT] Time initialization succeeded.\n");

        while(1){
            // If you do non-preemptive scheduling, it's used to surrender control
            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        } 
    }
    else{
        // init_global_cpu();      // init global_cpu struct
        // // Newly added, print cpu_id
        // printk("> [Current cpu_id]: %d\n", cpuid);

        // setup_exception();
        // printk("> [INIT-%d] Interrupt processing initialization succeeded.\n", cpuid);

        // init_time();
        // printk("> [INIT-%d] Time initialization succeeded.\n", cpuid);
        // while(1){}
    }
    return 0;
}