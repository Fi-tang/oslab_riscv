**写在最前**
本分支的任务与 P2-A_Simple_Kernel 的部分一致，新增加的功能是例外处理和时钟中断,因此不特别增加 guidebook 文件夹。
本次调试过程中的图片可以参考 "feishu_picture"
具体的设计内容可以参考 **feishu_coding** 文件夹下的 **"P2-part3-4 的总结"**，和 task3-part2 子目录下的 **"task3-前半部分总结"**
P2 最主要的两条汇编指令是 **sret** 和 **ecall**,
sret 的作用是: 从内核态返回用户态，返回用户态的地址写在 sepc 寄存器中，
对于系统调用而言，为了避免多次重复返回到系统调用行所在的指令，需要将 sepc 修改为 sepc + 4, 
而对于时钟中断等情况，则不需要设置 sepc 为新值。

ecall表示用户态触发系统调用，陷入内核。
sret 和 ecall 分别实现了内核态切换到用户态，和用户态陷入内核态的处理过程。

重点在于初始时加载进程 pcb 信息（应用程序还没有被执行时），如何设置进程切换 do_scheduler() 函数中的相关参数。
由于程序执行必须在用户态下，所以程序的第一次加载必须通过 do_scheduler() 设置返回地址 $ra(return address) 为 ret_from_exception,
利用 sret 指令执行用户程序。
而不是直接在 do_scheduler() 里执行 sret(只能满足程序初始时调用的场景，如果 do_scheduler() 是在两个争抢锁的用户进程中进行调度，
ra应该明确指向 while(1) 中的 do_scheduler() 结束的下一行，而不是指向 sys_mutex_acquire() 的下一行， 换言之，如果没有抢到锁，应该一直驻留在内核态的
锁判断循环中，而不是直接跳出内核态，指向用户程序的下一行)。

第二大部分容易出错的点是： SAVE_CONTEXT 和 RESTORE_CONTEXT
SAVE_CONTEXT, 将当前运行的寄存器等信息写入pcb 内存空间
sp指针需要在进入后修改为内核栈的指针
需要先写后用寄存器（否则寄存器的值会被改掉，导致写入内存的值不准确）

RESTORE_CONTEXT, 将pcb内存空间的值写回到 寄存器中
在退出之前，内核栈指针sp应该被保存
需要先用后写寄存器（否则寄存器被恢复的值不准确）

关注寄存器的值
![registers](https://github.com/Fi-tang/oslab_riscv/blob/Project2-A_Complex_Kernel/feishu_picture/part2-debugging/registers.PNG)

