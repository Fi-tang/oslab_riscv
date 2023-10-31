#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    long syscall_num = (long)(regs -> regs[10]);             // a0
    long handle_syscall_arg0 = (long)(regs -> regs[11]);     // a1
    long handle_syscall_arg1 = (long)(regs -> regs[12]);     // a2
    long handle_syscall_arg2 = (long)(regs -> regs[13]);     // a3
    long handle_syscall_arg3 = (long)(regs -> regs[14]);     // a4
    long handle_syscall_arg4 = (long)(regs -> regs[15]);     // a5
    
    syscall[syscall_num](handle_syscall_arg0, handle_syscall_arg1, handle_syscall_arg2, handle_syscall_arg3, handle_syscall_arg4);
}