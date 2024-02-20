写在最前：建议阅读的分支顺序 master -> P1 -> P2_simple -> P2_complex -> P3 -> P4
**guidebook** 文件夹记录了实验的内容和任务发布
**feishu_coding** 展示了代码过程中的设计思考和debug过程，展现的形式是 url 链接。

P1 的部分完成了操作系统的引导，镜像文件的加载，以及对ELF文件的处理
操作系统的引导分为 
1. BIOS阶段(CPU上电后跳到指定位置执行，此处记录我们的 Boot Loader)
2. Boot Loader 阶段: 将操作系统代码搬运并展开到内存。
3. OS阶段: 真正执行操作系统代码

本分支一共完成了 P1 的如下功能:
1. 制作第一个引导块
2. 加载和初始化内存，清空bss段，设置栈指针，跳转到内核 main 函数
3. 加载并选择启动多个用户程序之一：涉及 write_image_info()
4. 镜像文件的紧密排列

实验结果显示如下：可以看到通过汇编打印出了欢迎信息，通过输入数据打印出各个用户程序被调度执行的情况
实验同样支持通过程序名的字符输入进行调度。
P1的设计过程在 **readme.pdf** 中详细展示
![result](https://github.com/Fi-tang/oslab_riscv/blob/Project1-BootLoader/feishu_picture/task3/result.png)
