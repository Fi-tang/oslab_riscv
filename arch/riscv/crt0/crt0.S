#include <asm.h>

# define USER_STACK_BASE 0x53500000 
# define STACK_SIZE 0x10000
# define KERNEL_ENTER_POINT 0x50201000

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    lui t3, %hi(USER_STACK_BASE)
    addi t3, t3, %lo(USER_STACK_BASE)   // t3 = USER_STACK_BASE

    mv sp, t3                           // Every program has it's own stack_pointer  

    // clean bss segment
    mv t0,  a1                          // enterance address
    add t1, t0, a6                     // a6 has task_filesz, t1 = enterance_address + task_filesz
    add t2, t0, a7                     // a7 has task_memorysz, t2 = enterance_address + task_memorysz
    addi t2, t2, 1
    bge  t1, t2, bss_clean_done         // t1 >= t2 + 1 

bss_clean_loop:
    sw zero, 0(t1)                      // clean 
    addi t1, t1, 4                      // store word in 4
    blt t1, t2, bss_clean_loop

bss_clean_done:

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
