#include "spinlock.h"
#define NCPU 8
#define NPROC 64

typedef unsigned long uint64;
typedef uint64 *pagetable_t;

// Saved registers for kernel context switches.
struct context{
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

// Per-cpu state.
struct cpu {
    struct proc *proc; 
    // every cpu can hold an pointer to proc
    // The process running on this cpu, or null.
    struct context context;
    // swtch() --> enter scheduler().
    int noff;
    // turn on or turn off interrupt.
    int intena;
    // interrupts enabled before push_off() ?
};

extern struct cpu cpus[NCPU];

struct trapframe{
    uint64 kernel_satp;
    uint64 kernel_sp;
    uint64 kernel_trap;
    uint64 epc;
    uint64 kernel_hartid;
    uint64 ra;
    uint64 sp;
    uint64 gp;
    uint64 tp;
    uint64 t0;
    uint64 t1;
    uint64 t2;
    uint64 s0;
    uint64 s1;
    uint64 a0;
    uint64 a1;
    uint64 a2;
    uint64 a3;
    uint64 a4;
    uint64 a5;
    uint64 a6;
    uint64 a7;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
    uint64 t3;
    uint64 t4;
    uint64 t5;
    uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// per-process state
struct proc{
    struct spinlock lock;

    // p -> lock must be held when using these:
    enum procstate state;   // Process state.
    void *chan;             // If non-zero, sleeping on chan 
    int killed;             // If non-zero, have been killed
    int xstate;             // Exit status to be returned to parent's wait
    int pid;                // Process ID

    // wait_lock must be held when using this:
    struct proc *parent;    // Parent process

    // these are private to the process, so p -> lock need not be held.
    uint64 kstack;          // Virtual address of kernel_stack
    uint64 sz;              // Size of process memory(bytes)
    pagetable_t pagetable;  // User page table
    struct trapframe *trapframe; // data page for trampoline.S
    struct context context;      // swtch() here to run process
    char name[16];
};