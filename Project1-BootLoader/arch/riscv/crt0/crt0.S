#include <asm.h>

# define USER_STACK_BASE 0x53500000 
# define STACK_SIZE 0x10000
# define KERNEL_ENTER_POINT 0x50201000

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    // assume a7 records - taskid, now we calculate it
    lui t0, %hi(STACK_SIZE)
    addi t0, t0, %lo(STACK_SIZE)        // t0 = STACK_SIZE

    addi t1, a7, -1                    // t1 = (taskid - 1)
    mul t2, t0, t1                     // t2 = (taskid - 1) * STACK_SIZE

    lui t3, %hi(USER_STACK_BASE)
    addi t3, t3, %lo(USER_STACK_BASE)   // t3 = USER_STACK_BASE

    add t4, t3, t2                      // t4 = USER_STACK_BASE + (taskid - 1) * STACK_SIZE

    mv sp, t4                           // Every program has it's own stack_pointer  
                                        // sp = USER_STACK_BASE + (taskid - 1) * STACK_SIZE --> 0x535[taskid - 1]0000

    /* TODO: [p1-task3] enter main function */
    j main 
    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    lui t5, %hi(KERNEL_ENTER_POINT)
    addi t5, t5, %lo(KERNEL_ENTER_POINT)
    jr t5
    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
