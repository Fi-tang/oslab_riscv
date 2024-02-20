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

在这里做了双核的处理，对 do_scheudler() 的进程调度做了优化，设计 pid0 和 pid1 程序，作为没有其他进程调度时的程序使用，同时一旦有其他进程需要进行调度，就不再调度 pid0 或 pid1。一个好的方法是，并不将 pid0 和 pid1 加入 ready 队列，只改变进程状态。如果 ready 队列为空，说明需要调度 pid0 或 pid1, 将其从 task_ready 状态切换为 task_running 状态。

需要注意加锁的先后顺序，如果先加锁再保存现场，则寄存器的值发送了变化。
如果先恢复现场再解锁，则恢复的寄存器值会发生变化。
正确的顺序是: 1. 先保存现场再上锁 
              2. 先解锁再恢复现场
P3 的shell 启动效果如下:
可以看到屏幕上打印出了 cpu1 的信息
![shell](https://github.com/Fi-tang/oslab_riscv/blob/Project3-Interactive_OS_and_Process_Management/P3_result0.PNG)


多核启动的执行效果如下: 
