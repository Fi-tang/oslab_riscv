typedef unsigned int uint;

// Mutual exclusion lock.
struct spinlock{
    uint locked; // Is the lock held?
    
    char *name;
    struct cpu *cpu; // The cpu holding the lock.
};