#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  lui a0, %hi(exception_handler_entry)
  addi a0, a0, %lo(exception_handler_entry)
  csrw CSR_STVEC, a0 
  /* TODO: [p2-task4] enable interrupts globally */
  ret
ENDPROC(setup_exception)
