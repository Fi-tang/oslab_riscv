// src/lib.rs
mod timer;

// src/timer.rs

use crate::sbi::set_timer;
use riscv::register::{
    time,
    sie
};

// 当前已经触发多少次时钟中断
pub static mut TICKS: usize = 0;
// 触发时钟中断时间间隔
// 数值一般约为 cpu 频率的 1 %, 防止过多占用 cpu 资源
static TIMEBASE: u64 = 100000;

pub fn init(){
    unsafe{
        // 初始化时钟中断触发次数
        TICKS = 0;
        // 设置 sie 的 TI 使能 STIE 位
        sie:: set_stimer();
    }
    // 硬件机制问题，我们不能直接设置时钟中断触发间隔
    // 只能当每一次时钟中断触发时
    // 设置下一次时钟中断的触发时间
    // 设置为当前时间加上 TIMEBASE
    // 这次调用用来预处理
    clock_set_next_event();
    println("++++ setup timer! ++++");
}

pub fn clock_set_next_event(){
    // 调用 OpenSBI 提供的接口设置下次时钟中断触发事件
    set_timer(get_cycle() + TIMEBASE);
}

// 获取当前事件
fn get_cycle() -> u64 {
    time::read() as u64
}