#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    long syscall_num = (long)(regs -> regs[8]);             // s0
    long handle_syscall_arg0 = (long)(regs -> regs[9]);     // s1
    long handle_syscall_arg1 = (long)(regs -> regs[18]);    // s2
    long handle_syscall_arg2 = (long)(regs -> regs[19]);    // s3
    long handle_syscall_arg3 = (long)(regs -> regs[20]);    // s4
    long handle_syscall_arg4 = (long)(regs -> regs[21]);    // s5
    
    syscall[syscall_num](handle_syscall_arg0, handle_syscall_arg1, handle_syscall_arg2, handle_syscall_arg3, handle_syscall_arg4);
}