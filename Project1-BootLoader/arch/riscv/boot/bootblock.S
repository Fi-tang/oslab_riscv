#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"

	li a7, 9 					// a7 = #define BIOS_PUTSTR 9
	lui a0, %hi(msg)			// load msg(hi)
	addi a0, a0, %lo(msg)		// load msg(lo)
	
	jal bios_func_entry			// start calling port_write

	// TODO: [p1-task2] call BIOS to read kernel in SD card

	li a7, 11                         // #define BIOS_SDREAD 11
    	lui a0, %hi(kernel)               // put in memory address 0x50201000 kernel
   	addi a0, a0, %lo(kernel)
    	lui t0, %hi(os_size_loc)         // assign the address of os_size_loc in s0
    	addi t0,t0, %lo(os_size_loc)
    	lh a1, 0(t0)                     // num_of_blocks in 0x502001fc  t0 [0x502001fc] 
                                     // 0x502001fc: 15(assign 15 blocks)
    	li a2, 1                         // block_id: 1
    	jal bios_func_entry				 // start calling sd_read

	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j kernel 				        // jump to kernel, assign it never returns, so just use "j" instead of "jal"
	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

