#include "timer1.h"

// 100us 定时中断
void timer1_init(void)
{
    T1CR = 0x03; // Fcpu 8分频
    // T1CNT = 100 - 1; // 计数值
    T1LOAD = 100 - 1; // 100us
    T1EN = 1;
    T1IE = 1;
}