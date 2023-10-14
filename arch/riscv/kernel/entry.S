#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

#define PCB_SWITCH_TO_CONTEX_OFFSET 56  

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

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */

.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */
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
  sd ra, 0(t0)
  sd sp, 8(t0)
  sd s0, 16(t0)
  sd s1, 24(t0)
  sd s2, 32(t0)
  sd s3, 40(t0)
  sd s4, 48(t0)
  sd s5, 56(t0)
  sd s6, 64(t0)
  sd s7, 72(t0)
  sd s8, 80(t0)
  sd s9, 88(t0)
  sd s10, 96(t0)
  sd s11, 104(t0)

  addi t1, a1, PCB_SWITCH_TO_CONTEX_OFFSET
  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  ld ra, 0(t1)
  ld sp, 8(t1)
  ld s0, 16(t1)
  ld s1, 24(t1)
  ld s2, 32(t1)
  ld s3, 40(t1)
  ld s4, 48(t1)
  ld s5, 56(t1)
  ld s6, 64(t1)
  ld s7, 72(t1)
  ld s8, 80(t1)
  ld s9, 88(t1)
  ld s10, 96(t1)
  ld s11, 104(t1)
  mv tp, a1
  // addi sp, sp, SWITCH_TO_SIZE
  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */


  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */


  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */


ENDPROC(exception_handler_entry)
