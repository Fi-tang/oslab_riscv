/**
 * 需要解决时钟中断的问题。
 * Q1. 时钟中断的触发需要设置哪些CSR寄存器?
 *  这个比较容易，根据 sip(Supervisor interrupt pending), 表示监管者申请执行的指令，
 * 设置 sip 的 STIP = 1, 当执行完一条指令时，如果发现 sip 的 STIP = 1, 那么如果此时
 * sie(Supervisor interrupt enable) 使能位同样有 sie 的 STIE = 1, 
 * 表示可以执行时钟中断
 * 
 * Q2. 时钟中断的触发过程设计?
 * 根据 rCore 的说明，时钟中断不能设置外部的时钟中断触发间隔。
 * 第一次主动触发时钟中断，并获取当前的时钟周期数, get_cycles()
 * 下一次的时钟中断触发时间为: get_cycles() + TIME_BASE
 * 
 * Q3. 如何设计时钟的初始化代码?
 *    根据 第一遍写的 C 代码与 rCore 比较
*/
# define STIP 0x00000005
# define STIE 0x00000005
# define TIME_BASE 10000

typedef struct time_related_regs{
    unsigned long sie;
    unsigned long sip;
    unsigned long time;    
} time_related_regs;

extern time_related_regs regs;

unsigned long get_current_cycle(){
    return regs.time;
}

void set_stip(){
    regs.sip = STIP;    // ready to request   
}

void do_scheduler(){
    printk("do - scheduler!\n");
}

void set_time_interrupt(unsigned long time){
    
}

void time_interrupt(){
    set_time_interrupt(get_current_cycle() + TIME_BASE);
    do_scheduler();
}

void time_interrupt_init(){
    set_stip();
    if(regs.sie = STIE){
        time_interrupt();
    }
}