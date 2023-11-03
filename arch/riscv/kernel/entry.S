#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

#define PCB_SWITCH_TO_CONTEX_OFFSET   56  
#define PCB_USER_REGS_CONTEXT_OFFSET  168 

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */
  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  addi t0, tp, PCB_USER_REGS_CONTEXT_OFFSET
  
  sd zero,  OFFSET_REG_ZERO(t0)
  sd ra,    OFFSET_REG_RA(t0)
  sd sp,    OFFSET_REG_SP(t0)
  sd gp,    OFFSET_REG_GP(t0)
  sd tp,    OFFSET_REG_TP(t0)
  sd t0,    OFFSET_REG_T0(t0)
  sd t1,    OFFSET_REG_T1(t0)
  sd t2,    OFFSET_REG_T2(t0)
  sd s0,    OFFSET_REG_S0(t0)
  sd s1,    OFFSET_REG_S1(t0)

  sd a0,    OFFSET_REG_A0(t0)
  sd a1,    OFFSET_REG_A1(t0)
  sd a2,    OFFSET_REG_A2(t0)
  sd a3,    OFFSET_REG_A3(t0)
  sd a4,    OFFSET_REG_A4(t0)
  sd a5,    OFFSET_REG_A5(t0)
  sd a6,    OFFSET_REG_A6(t0)
  sd a7,    OFFSET_REG_A7(t0)
  sd s2,    OFFSET_REG_S2(t0)
  sd s3,    OFFSET_REG_S3(t0)

  sd s4,    OFFSET_REG_S4(t0)
  sd s5,    OFFSET_REG_S5(t0)
  sd s6,    OFFSET_REG_S6(t0)
  sd s7,    OFFSET_REG_S7(t0)
  sd s8,    OFFSET_REG_S8(t0)
  sd s9,    OFFSET_REG_S9(t0)
  sd s10,   OFFSET_REG_S10(t0)
  sd s11,   OFFSET_REG_S11(t0)
  sd t3,    OFFSET_REG_T3(t0)
  sd t4,    OFFSET_REG_T4(t0)

  sd t5,    OFFSET_REG_T5(t0)
  sd t6,    OFFSET_REG_T6(t0)

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */
  csrr s0,  CSR_SSTATUS
  sd s0,    OFFSET_REG_SSTATUS(t0)

  csrr s0, CSR_SEPC
  sd s0,  OFFSET_REG_SEPC(t0)

  csrr s0, CSR_SCAUSE
  sd s0,  OFFSET_REG_SCAUSE(t0)
.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
  addi t0, tp, PCB_USER_REGS_CONTEXT_OFFSET

  ld zero,  OFFSET_REG_ZERO(t0)
  ld ra,    OFFSET_REG_RA(t0)
  ld sp,    OFFSET_REG_SP(t0)
  ld gp,    OFFSET_REG_GP(t0)
  ld tp,    OFFSET_REG_TP(t0)
  
  ld t1,    OFFSET_REG_T1(t0)
  ld t2,    OFFSET_REG_T2(t0)
 
  ld s1,    OFFSET_REG_S1(t0)

  ld a0,    OFFSET_REG_A0(t0)
  ld a1,    OFFSET_REG_A1(t0)
  ld a2,    OFFSET_REG_A2(t0)
  ld a3,    OFFSET_REG_A3(t0)
  ld a4,    OFFSET_REG_A4(t0)
  ld a5,    OFFSET_REG_A5(t0)
  ld a6,    OFFSET_REG_A6(t0)
  ld a7,    OFFSET_REG_A7(t0)
  ld s2,    OFFSET_REG_S2(t0)
  ld s3,    OFFSET_REG_S3(t0)

  ld s4,    OFFSET_REG_S4(t0)
  ld s5,    OFFSET_REG_S5(t0)
  ld s6,    OFFSET_REG_S6(t0)
  ld s7,    OFFSET_REG_S7(t0)
  ld s8,    OFFSET_REG_S8(t0)
  ld s9,    OFFSET_REG_S9(t0)
  ld s10,   OFFSET_REG_S10(t0)
  ld s11,   OFFSET_REG_S11(t0)
  ld t3,    OFFSET_REG_T3(t0)
  ld t4,    OFFSET_REG_T4(t0)

  ld t5,    OFFSET_REG_T5(t0)
  ld t6,    OFFSET_REG_T6(t0)

  ld s0,    OFFSET_REG_SSTATUS(t0)
  csrw CSR_SSTATUS, s0 

  ld s0,    OFFSET_REG_SEPC(t0)
  csrw CSR_SEPC, s0

  ld s0,    OFFSET_REG_SCAUSE(t0)
  csrw CSR_SCAUSE, s0

  ld t0,    OFFSET_REG_T0(t0)
  ld s0,    OFFSET_REG_S0(t0)
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  // addi sp, sp, -(SWITCH_TO_SIZE)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/
  
  addi t0, a0, PCB_SWITCH_TO_CONTEX_OFFSET
  // save context 
  mv tp, a0
  sd ra,  SWITCH_TO_RA(t0)
  sd sp,  SWITCH_TO_SP(t0)
  sd s0,  SWITCH_TO_S0(t0)
  sd s1,  SWITCH_TO_S1(t0)
  sd s2,  SWITCH_TO_S2(t0)
  sd s3,  SWITCH_TO_S3(t0)
  sd s4,  SWITCH_TO_S4(t0)
  sd s5,  SWITCH_TO_S5(t0)
  sd s6,  SWITCH_TO_S6(t0)
  sd s7,  SWITCH_TO_S7(t0)
  sd s8,  SWITCH_TO_S8(t0)
  sd s9,  SWITCH_TO_S9(t0)
  sd s10, SWITCH_TO_S10(t0)
  sd s11, SWITCH_TO_S11(t0)

  addi t1, a1, PCB_SWITCH_TO_CONTEX_OFFSET
  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  ld ra, SWITCH_TO_RA(t1)
  ld sp, SWITCH_TO_SP(t1)
  ld s0, SWITCH_TO_S0(t1)
  ld s1, SWITCH_TO_S1(t1)
  ld s2, SWITCH_TO_S2(t1)
  ld s3, SWITCH_TO_S3(t1)
  ld s4, SWITCH_TO_S4(t1)
  ld s5, SWITCH_TO_S5(t1)
  ld s6, SWITCH_TO_S6(t1)
  ld s7, SWITCH_TO_S7(t1)
  ld s8, SWITCH_TO_S8(t1)
  ld s9, SWITCH_TO_S9(t1)
  ld s10, SWITCH_TO_S10(t1)
  ld s11, SWITCH_TO_S11(t1)
  mv tp, a1
  // addi sp, sp, SWITCH_TO_SIZE
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT
  csrr a0, CSR_SCAUSE
  blt a0, zero, interrupt_case
  li a1, 0x8
  bne a0, a1, interrupt_case

  csrr s0, CSR_SEPC
  addi s0, s0, 4
  csrw CSR_SEPC, s0
interrupt_case: 
  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT

  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  mv t0, tp 
  addi t0, t0, PCB_USER_REGS_CONTEXT_OFFSET

  sd a0, OFFSET_REG_A0(t0)
  sd a1, OFFSET_REG_A1(t0)
  sd a2, OFFSET_REG_A2(t0)
  sd a3, OFFSET_REG_A3(t0)
  sd a4, OFFSET_REG_A4(t0)
  sd a5, OFFSET_REG_A5(t0)

  csrr s0, CSR_SEPC
  sd s0, OFFSET_REG_SEPC(t0)

  ld a0, OFFSET_REG_REGS_POINTER(t0)
  csrr a1, CSR_STVAL 
  csrr a2, CSR_SCAUSE
  call interrupt_helper
  j ret_from_exception 
ENDPROC(exception_handler_entry)