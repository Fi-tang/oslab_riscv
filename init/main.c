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
    pcb[alloc_index].kernel_sp = allocPage(1);      // need to notice that kernel_sp allocate from 0xffffffc052001000
    pcb[alloc_index].user_sp = allocPage(1);

    pcb[alloc_index].kernel_stack_base = pcb[alloc_index].kernel_sp;
    pcb[alloc_index].user_stack_base = pcb[alloc_index].user_sp;

    pcb[alloc_index].cursor_x = alloc_index;
    pcb[alloc_index].cursor_y = alloc_index;
    Initialize_QueueNode(&pcb[alloc_index].list);
    Initialize_QueueNode(&pcb[alloc_index].wait_list);

    pcb[alloc_index].pid = alloc_index;
    strcpy(pcb[alloc_index].name, name);
    long current_task_entry_address = load_task_img_by_name(task_num, pcb[alloc_index].name);
    pcb[alloc_index].status = TASK_RUNNING;
    pcb[alloc_index].pcb_mask = 0x3;
    init_pcb_regs(&pcb[alloc_index].pcb_switchto_context, &pcb[alloc_index].pcb_user_regs_context, &pcb[alloc_index], current_task_entry_address);
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
    //do_exec("shell", 0, NULL);
}

void do_writeArgvToMemory(pcb_t *pcb, int argc, char *argv[]){
    if(argc == 0 || (argc == 1 && argv == NULL)){
        pcb -> pcb_user_regs_context.regs[10] = argc;   // need to pass the assert(argc >= 1) test, where argv[0] == taskname
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
            pcb[i].kernel_sp = allocPage(1);
            pcb[i].user_sp = allocPage(1);

            pcb[i].kernel_stack_base = pcb[i].kernel_sp;
            pcb[i].user_stack_base = pcb[i].user_sp;

            pcb[i].cursor_x = i;
            pcb[i].cursor_y = i;
            Initialize_QueueNode(&pcb[i].list);
            Initialize_QueueNode(&pcb[i].wait_list);

            strcpy(pcb[i].name, name);
            long current_task_entry_address = load_task_img_by_name(task_num, name);
            pcb[i].status = TASK_READY;
             
            // If otherwise declared, inhereit father's mask
            int current_cpu = get_current_cpu_id();
            pcb_t *father_pcb_node = global_cpu[current_cpu].cpu_current_running;
            // default mask from father
            pcb[i].pcb_mask = father_pcb_node -> pcb_mask;

            do_writeArgvToMemory(&pcb[i], argc, argv);

            init_pcb_regs(&pcb[i].pcb_switchto_context, &pcb[i].pcb_user_regs_context, &pcb[i], current_task_entry_address);

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

static void clean_boot_address_map(){
    PTE *early_pgdir = (PTE *)pa2kva(PGDIR_PA);
    uint64_t va = 0x50000000lu;
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));

    uint64_t physical_address = get_pa(early_pgdir[vpn2]);
    clear_pgdir(pa2kva(physical_address)); // clean 2th page_table

    early_pgdir[vpn2] = 0;

}

//**********************The following used for allocate user program *****************
// Step1: allocate user_pgdir(corresponding to level_one_pgdir)
void allocate_user_pgdir(pcb_t *pcb){
    struct SentienlNode *malloc_user_pgdir_sentienl = (struct SentienlNode *)kmalloc(1);
    printl("allocate_user_pgdir:\n");
    print_page_alloc_info(malloc_user_pgdir_sentienl);

    pcb -> user_pgdir_kva = pa2kva(malloc_user_pgdir_sentienl -> head -> physical_address);
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!![Warning]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // kmalloc(1) is free to clear_pgdir, but need to pay attention to the usage of nexr pointer, we may further need it
}

// Step2: Copy kernel pgdir to user_pgdir
// only need to copy level_one_pgdir item, the level_two_pgdir can be relocated
void copy_kernel_pgdir_to_user_pgdir(uintptr_t dest_pgdir, uintptr_t src_pgdir){
    // Step1:
    uint64_t va = (0xffffffc050000000lu) & VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    PTE *kernel_pgdir = (PTE *)dest_pgdir;
    PTE *user_pgdir = (PTE *)src_pgdir;
    user_pgdir[vpn2] = kernel_pgdir[vpn2];
}

/**
this function is to map three level user_page
(1) level_one_pgdir, has been allocated
(2) level_two_pgdir, need to call kmalloc(1)
(3) level_three_pgdir, need to call kmalloc(2)

debug error: 
*********** first record *****************************************************
because we assign every page as struct ListNode *, with physical_address,
so level_one_pgdir 0xffffffc052003000 looks like: 0x52003000 00000000 00000000 00000000
it did not pass the test that level_one_pgdir[vpn2] == 0
*/
void map_single_user_page(uint64_t va, uint64_t pa, PTE *level_one_pgdir){
    printl("mapping 0x%x to 0x%x\n", va, pa);
    printl("[map_single_user_page]: level_one_pgdir 0x%x\n", (uintptr_t)level_one_pgdir); // debug

    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn1 << PPN_BITS) ^ (va >> NORMAL_PAGE_SHIFT);

    if(level_one_pgdir[vpn2] == 0){ // have not allocated level_two_pgdir
        struct SentienlNode *malloc_level_two = (struct SentienlNode *)kmalloc(1);
        print_page_alloc_info(malloc_level_two);         // debug
        uint64_t return_level_two_address = malloc_level_two -> head -> physical_address;
        printl("[map_single_user_page]: level_two_pgdir 0x%x\n", pa2kva(return_level_two_address)); // debug
        set_pfn(&level_one_pgdir[vpn2], return_level_two_address >> NORMAL_PAGE_SHIFT);
        set_attribute(&level_one_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(level_one_pgdir[vpn2])));     // clean level_two_pgdir, for furture fullfill
    }
    PTE *level_two_pgdir = (PTE *)pa2kva(get_pa(level_one_pgdir[vpn2]));
    if(level_two_pgdir[vpn1] == 0){  // have not allocated level_three_pgdir
        struct SentienlNode *malloc_level_three = (struct SentienlNode *)kmalloc(1);
        print_page_alloc_info(malloc_level_three);     // debug
        uint64_t return_level_three_address = malloc_level_three -> head -> physical_address;
        printl("[map_single_user_page]: level_three_pgdir 0x%x\n", pa2kva(return_level_three_address)); // debug
        set_pfn(&level_two_pgdir[vpn1], return_level_three_address >> NORMAL_PAGE_SHIFT);
        set_attribute(&level_two_pgdir[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(level_two_pgdir[vpn1])));     // clean level_three_pgdir, for furture fullfill
    }
    PTE *level_three_pgdir = (PTE *)pa2kva(get_pa(level_two_pgdir[vpn1]));
    set_pfn(&level_three_pgdir[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &level_three_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);
}

// Step3: load task-info and map_pages
// get information from tasks[task_id], map its user_page
void map_user_page(pcb_t *pcb, int task_id){

}

void do_unit_test(){
    allocate_user_pgdir(&pcb[0]);
    copy_kernel_pgdir_to_user_pgdir(pa2kva(PGDIR_PA), pcb[0].user_pgdir_kva);
    // we grantuee that kmalloc is correct
    map_single_user_page(0x100000, 0x54000000, (PTE *)(pcb[0].user_pgdir_kva));
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    int cpuid = get_current_cpu_id();
    if(cpuid == 0){
        clean_boot_address_map(); 

        init_global_cpu();

        init_jmptab();
        // Init task information (〃'▽'〃)
        init_task_info();
        // Init Process Control Blocks |•'-'•) ✧
        // only used for printk
        // init_pcb_loop();
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
        do_unit_test();
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
        
        // init_time();
        kernel_spin_lock_release();

        while(1){
            // enable_preempt();
            // asm volatile("wfi");
        }
    }
    return 0;
}