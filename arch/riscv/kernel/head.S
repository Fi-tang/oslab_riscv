/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0xffffffc052000000
#define SUBCORE_STACK   0xffffffc053000000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr a0, CSR_MHARTID
  bnez a0, second_core    // sub-core should not clean-bss again!!!

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
  li sp, KERNEL_STACK // assign stack pointer

  j main     // jump to main function 

second_core:
  li sp, SUBCORE_STACK

  j main

loop:
  wfi
  j loop

END(_start)