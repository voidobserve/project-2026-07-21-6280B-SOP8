#include "timer0.h"

void timer0_init(void)
{
    /*
        T0 时钟源为 FCPU / 2 == (8MHz) / 2 == 4MHz
        4MHz / 64 分频 == 62.5KHz
        1s / 62.5KHz == 0.000016 s == 16us
        计数一次相当于 0.000016 s，16us

        但是实际测试，T0的时钟源是 8MHz的，
        8MHz / 64 分频 == 125KHz
        1u / 125KHz == 8us
        8us * 125次计数 == 1ms
    */
    T0CR = 0x05; // 时钟为CPU时钟，定时器 64 分频
    T0CNT = (u8)(255 - 125);
    T0IE = 1;
}