/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la t0, edata       	 // i = __bss_start
  la t1, end       	//  __BSS_END__ 
  addi t1,t1, 1             // __BSS_END__ + 1
  bge t0,t1, clean_done     // i >= __BSS_END__ + 1
clean_loop:
  sw zero, 0(t0)            // clean 
  addi t0,t0, 4             // the next 4 Byte 
  blt t0, t1, clean_loop

clean_done: 

  /* TODO: [p1-task2] setup C environment */
  lui sp, %hi(KERNEL_STACK)
  addi sp,sp, %lo(KERNEL_STACK)  // assign stack pointer

  jal ra, main     // jump to main function 

loop:
  wfi
  j loop

END(_start)
