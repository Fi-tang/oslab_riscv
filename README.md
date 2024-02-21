**写在最前**
P4 和之前的内容一致，关于虚拟内存的内存划分与布局，需要参考 **feishu_coding** 文件夹，
关于调试中遇到的问题以及设计过程截图，参考 **feishu_picture** 文件夹，
主要的虚拟内存内容在 **guidebook** 文件夹中。

P1-P3 中，对于每个进程地址空间的处理，都是通过事先编译来确定的，无法动态地加载进程到任意一个空闲的地址区域。
P4 需要自己实现，按照页(4KB为一页）来进行内存空间的分配，启动虚拟内存机制进入内核，
实现内存隔离机制，按照 RISC-V 的 Sv39 格式进行三级页表的设计，并使用虚拟地址进行访问。
从这里开始，操作系统操作的地址都是虚拟地址，页表中只填写物理地址。

关于三级页表的查找过程可以参考: 
![three_level_pgtable](https://github.com/Fi-tang/oslab_riscv/blob/Project4-Virtual_Memory_Management/address_relocate.png)

关于实际的物理内存布局，可以参考 **feishu_coding** 子文件夹下的 **P4**, **P4-design review**
,实际上在 start_code 中约定了内核页表被固定放在 0xffff_ffc0_5100_0000lu 的位置，关于 kmalloc 和
kinit 的部分，参考了 xv6 中的设计实现，直接将每个4KB 页的前8个字节作为链表的 next 指针，记录 head 指针，进行分配和回收。其余的二、三级页表和对应的物理页面都是按需分配，从 0xffff_ffc0_5100_3000 的位置开始分配。
也可以使用 tail -f ~/OSlab/oslab.txt 的命令 进行日志的输出，打印具体的页面分配情况。


P4 的实现结果如下所示,也可以在 gdb 中发现对应的内核代码地址均为 0xffffffc0XXXXX 等进行表示。
![result](https://github.com/Fi-tang/oslab_riscv/blob/Project4-Virtual_Memory_Management/P4_result.PNG)

