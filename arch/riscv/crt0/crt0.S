#include <asm.h>

# define STACK_SIZE 0x10000
# define KERNEL_ENTER_POINT 0x50201000
# define USER_STACK_P1 0x52500000

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    // Every program has it's own stack_pointer  

    /* TODO: [p1-task3] enter main function */
    jal main 
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
