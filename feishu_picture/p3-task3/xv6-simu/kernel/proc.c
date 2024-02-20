#include "proc.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void ret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

struct spinlock wait_lock;