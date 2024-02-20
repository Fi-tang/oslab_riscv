**写在最前**
与其他分支类似，**guidebook** 文件夹中记录了详细的任务内容，
**feishu_coding** 文件夹中记录了程序的设计，design review 和 调试过程，
**feishu_picture** 文件夹是对应的调试设计中的图片。

P3 需要完成进程的管理，通信和多核执行。
总共有如下功能:
1. 实现简易的 shell, 完成 OS和用户的交互，以及　clear, ps, exec 等终端命令
2. 实现 barrier(屏障)，信号量(semaphore), 条件变量(condition), mailbox 等通信方式
3. 实现双核启动，利用 send_ipi 发送核间中断。
4. 实现 taskset 的动态绑核指令。

P3 整体而言难度不大，需要注意的是在多核启动的过程中，send_ipi 的核间中断会同时向主核与从核发出，
从核顺利响应中断，主核不需要响应中断，但应该及时取消掉 sip 寄存器上的标志位(sip 表示内核态下尚未被处理的中断) pending。
如果没有取消 sip 对应的标志位，会导致操作系统卡在该处。
