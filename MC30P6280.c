/******************************************************************************
;  *   	@型号				  : MC30P6280
;  *   	@创建日期             : 2021.08.04
;  *   	@公司/作者			  : SINOMCU-FAE
;  *   	@晟矽微技术支持       : 2048615934
;  *   	@晟矽微官网           : http://www.sinomcu.com/
;  *   	@版权                 : 2021 SINOMCU公司版权所有.
;  *----------------------摘要描述---------------------------------
******************************************************************************/
#include "user.h"

u8 volatile abuf;
u8 volatile statusbuf;

volatile bit_flag_t flag1;
volatile bit_flag_t flag2;

// 当前切换到的电池电压比较挡位：
volatile bat_lev_t cur_cmp_bat_lev = 0;
/*
    临时的电池电压挡位

    刚上电时，或者是刚从低功耗期间唤醒，需要给它一个初始的默认值
*/
volatile bat_lev_t tmp_bat_lev = 0;
volatile bat_lev_t bat_lev = 0; // 稳定之后的、电池电压挡位

volatile u16 pwr_off_cnt = 0; // xx min 自动关机的倒计时
/*
    设备没有开机、并且没有在充电，累计计数，
    满足一定时间后进入低功耗
*/
volatile u8 into_low_power_cnt = 0;

// 控制充电动画 (REVIEW: 刚进入充电时，要注意清零)
volatile u8 charge_anim_phase = 0;

// 充满电的计数值(REVIEW: 刚进入充电时，要注意清零)
volatile u16 charge_fully_cnt = 0;

volatile u8 led_sta_refresh_cnt = 0;

static volatile u8 cur_lvd_state = 0;

static volatile u8 last_key_id = KEY_ID_NONE;
static volatile u8 press_cnt = 0;               // 按键按下的时间计数
static volatile u8 filter_cnt = 0;              // 按键消抖，使用的变量
static volatile u8 filter_key_id = KEY_ID_NONE; // 按键消抖时使用的变量
// 按键短按松开之后，记录时间，用于判断这段时间内是否有再次按下
static volatile u8 click_delay_cnt = 0;
static volatile u8 click_cnt = 0; // 记录按键连击的次数
volatile u8 cur_key_id = KEY_ID_NONE;

/************************************************
;  *    @Function Name       : C_RAM
;  *    @Description         : 初始化RAM
;  *    @IN_Parameter      	 :
;  *    @Return parameter    :
;  ***********************************************/
void C_RAM(void)
{
    __asm;
    movai 0x3F;
    movra FSR;
    movai 47;
    movra 0x3F;
    decr FSR;
    clrr INDF;
    djzr 0x3F;
    goto $ - 3;
    clrr 0x3F;
    __endasm;
}

/************************************************
;  *    @Function Name       : IO_Init
;  *    @Description         :
;  *    @IN_Parameter      	 :
;  *    @Return parameter    :
;  ***********************************************/
void IO_Init(void)
{
    P1 = 0x00;    // 1:input 0:output
    DDR1 = 0x00;  // 1:input 0:output
    PUCON = 0xff; // 0:Effective 1:invalid
    PDCON = 0xff; // 0:Effective 1:invalid
    ODCON = 0x00; // 0:推挽输出  1:开漏输出
}

// 手册说切换LVD挡位后，至少隔200us，LVD的输出才有效：
// 可以隔1ms再调用一次该函数
void lvd_scan(void)
{
    // static u8 cur_lvd_state = 0;

    LVDEN = 1; // 使能 LVD

    switch (cur_lvd_state) {
    case LVD_LEV_NONE:
    case LVD_LEV_2V9:
        // 当前变量是默认值0,或者是已经切换完一轮挡位,重新从4.2V开始切换挡位
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_4V2;
        cur_lvd_state = LVD_LEV_4V2_SWITCHING;
        break;

    case LVD_LEV_4V2:
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_4V0;
        cur_lvd_state = LVD_LEV_4V0_SWITCHING;
        break;

    case LVD_LEV_4V0:
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_3V6;
        cur_lvd_state = LVD_LEV_3V6_SWITCHING;
        break;

    case LVD_LEV_3V6:
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_3V3;
        cur_lvd_state = LVD_LEV_3V3_SWITCHING;
        break;

    case LVD_LEV_3V3:
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_3V2;
        cur_lvd_state = LVD_LEV_3V2_SWITCHING;
        break;

    case LVD_LEV_3V2:
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_3V0;
        cur_lvd_state = LVD_LEV_3V0_SWITCHING;
        break;

    case LVD_LEV_3V0:
        MCR &= ~MCR_LVD_CFG_ALL;
        MCR |= MCR_LVD_CFG_2V9;
        cur_lvd_state = LVD_LEV_2V9_SWITCHING;
        break;
        // ==========================================================
        // ==========================================================

    case LVD_LEV_4V2_SWITCHING:
        cur_lvd_state = LVD_LEV_4V2;
        cur_cmp_bat_lev = BAT_LEV_4V2;
        break;
    case LVD_LEV_4V0_SWITCHING:
        cur_lvd_state = LVD_LEV_4V0;
        cur_cmp_bat_lev = BAT_LEV_4V0;
        break;
    case LVD_LEV_3V6_SWITCHING:
        cur_lvd_state = LVD_LEV_3V6;
        cur_cmp_bat_lev = BAT_LEV_3V6;
        break;
    case LVD_LEV_3V3_SWITCHING:
        cur_lvd_state = LVD_LEV_3V3;
        cur_cmp_bat_lev = BAT_LEV_3V3;
        break;
    case LVD_LEV_3V2_SWITCHING:
        cur_lvd_state = LVD_LEV_3V2;
        cur_cmp_bat_lev = BAT_LEV_3V2;
        break;
    case LVD_LEV_3V0_SWITCHING:
        cur_lvd_state = LVD_LEV_3V0;
        cur_cmp_bat_lev = BAT_LEV_3V0;
        break;
    case LVD_LEV_2V9_SWITCHING:
        cur_lvd_state = LVD_LEV_2V9;
        cur_cmp_bat_lev = BAT_LEV_2V9;
        break;
    }

    if (cur_lvd_state == LVD_LEV_4V2 || cur_lvd_state == LVD_LEV_4V0 ||
        cur_lvd_state == LVD_LEV_3V6 || cur_lvd_state == LVD_LEV_3V3 ||
        cur_lvd_state == LVD_LEV_3V2 || cur_lvd_state == LVD_LEV_3V0 ||
        cur_lvd_state == LVD_LEV_2V9) {
        if (LVDF) {
            /*
                如果当前 VDD电压 小于 BAT_LEV_XX ,
                但 记录的挡位 大于等于 BAT_LEV_XX ,
                将 记录的挡位 下调一级
            */
            if (tmp_bat_lev >= cur_cmp_bat_lev) {
                // 防止下溢：
                if (cur_cmp_bat_lev != BAT_LEV_2V9) {
                    tmp_bat_lev -= 1;
                } else {
                    tmp_bat_lev = BAT_LEV_2V9;
                }
            }
        } else {
            /*
                如果当前检测 VDD电压 不小于 该挡位
                但 记录的挡位 小于 该挡位
                将 记录的挡位 上调一级

                如果 记录的档位 等于 该档位
                不作修改
            */
            if (tmp_bat_lev < cur_cmp_bat_lev) {
                // 防止上溢：
                if (cur_cmp_bat_lev != BAT_LEV_4V2) {
                    tmp_bat_lev += 1;
                } else {
                    tmp_bat_lev = BAT_LEV_4V2;
                }
            }
        }
    }
}

/************************************************
;  *    @Function Name       : Sys_Init
;  *    @Description         : 系统初始化
;  *    @IN_Parameter      	 :
;  *    @Return parameter    :
;  ***********************************************/
void Sys_Init(void)
{
    GIE = 0;
    C_RAM();
    IO_Init();
    GIE = 1;
}

// 毫秒级延时
// 前提条件：FCPU = FHIRC / 1 / 2 == 8MHz
void delay_ms(u32 xms)
{
    while (xms) {
        u16 i = 531;
        while (i--) {
            Nop();
        }
        xms--;
    }
}

//=================================================================================

void user_init(void)
{
    // 按键检测引脚配置 P10
    PUCON &= ~(0x01 << 0); // 上拉电阻 20K
    DDR1 |= 0x01 << 0;     // 输入模式

    // 控制笔头的电源引脚配置 P14
    DDR1 |= 0x01 << 4; // P14 输入模式

    // 滚珠检测引脚配置 P15
    DDR1 |= 0x01 << 5; // 输入模式

    // 充电检测引脚
    DDR1 |= 0x01 << 3;

    timer0_init();
    timer1_init();

#if USER_DEBUG_ENABLE
    // P14 控制笔头的电源引脚配置为输出模式，作为测试引脚
    DDR1 &= ~(0x01 << 4);

    PUCON &= ~(0x01 << 2); // 上拉电阻 20K
    DDR1 |= (0x01 << 2);   // P12 输入模式
#endif

    delay_ms(1); // 等待系统稳定
}

void pen_pwr_on(void)
{
    DDR1 &= ~(0x01 << 4); // 输出模式
    P14D = 0;             // 输出低电平
}

void pen_pwr_off(void)
{
    PUCON |= (0x01 << 4); // 不使能上拉
    PDCON |= (0x01 << 4); // 不使能下拉
    DDR1 |= (0x01 << 4);  // P14 输入模式
}

// 关闭所有led
void led_all_off(void)
{
    // 关闭所有LED驱动引脚的上下拉：
    // 关闭 按键扫描的上拉、LED控制的上拉
    // 可能是唤醒之后芯片没有关掉led的上拉，导致之后无法进入低功耗，这里重新关闭一次上拉：
    PUCON |= (0x01 << 0 | 0x01 << 1 | 0x01 << 2);

    // 所有led驱动引脚配置为 输入模式
    DDR1 |= (0x01 << 0) | // P10
            (0x01 << 1) | // P11
            (0x01 << 2);  // P12
}

#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
// 只有四个指示灯的设备，才有 led1 的接口

void led1_on(void)
{
    // 不使用的引脚配置为输入模式，如果每次都关所有灯，再点亮指定灯，这一步可以省

    // LED公共端配置为输出，输出低电平
    // P10 LED 公共端
    P1 &= ~(0x01 << 0);
    DDR1 &= ~(0x01 << 0); // 输出模式

    // LED B端 配置为输出，输出高电平
    // P11 LED B端
    P1 |= (0x01 << 1);    // P11 输出高电平
    DDR1 &= ~(0x01 << 1); // P11 输出模式
}
#endif

void led2_on(void)
{
    // 不使用的引脚配置为输入模式，如果每次都关所有灯，再点亮指定灯，这一步可以省

    // B端 输出低电平， 公共端 输出高电平
    // P11 LED B端
    P1 &= ~(0x01 << 1);   // P11 输出低电平
    DDR1 &= ~(0x01 << 1); // P11 输出模式

    // P10 LED 公共端
    P1 |= (0x01 << 0);    // P10 输出高电平
    DDR1 &= ~(0x01 << 0); // P10 输出模式
}

void led3_on(void)
{
    // 不使用的引脚配置为输入模式，如果每次都关所有灯，再点亮指定灯，这一步可以省
    // A端 输出低电平，公共端 输出高电平
    // P12 LED A端
    P1 &= ~(0x01 << 2);   // P12 输出低电平
    DDR1 &= ~(0x01 << 2); // P12 输出模式
    // P10 LED 公共端
    P1 |= (0x01 << 0);    // P10 输出高电平
    DDR1 &= ~(0x01 << 0); // P10 输出模式
}

void led4_on(void)
{
    // 不使用的引脚配置为输入模式，如果每次都关所有灯，再点亮指定灯，这一步可以省
    // 公共端 输出低电平，A端 输出高电平
    // P10 LED 公共端
    P1 &= ~(0x01 << 0);   // P10 输出低电平
    DDR1 &= ~(0x01 << 0); // P10 输出模式
    // P12 LED A端
    P1 |= (0x01 << 2);    // P12 输出高电平
    DDR1 &= ~(0x01 << 2); // P12 输出模式
}

#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P003_JW))
// 按键双击来进行开关机

// 按键检测，放在10ms定时器中断处理
void key_scan(void)
{
    if (flag_is_in_charging) {
        // 充电期间，不检测按键
        last_key_id = 0;
        press_cnt = 0;
        filter_cnt = 0;
        filter_key_id = 0;
        click_delay_cnt = 0;
        click_cnt = 0;
        cur_key_id = 0;
        return;
    }

    led_all_off();
#if 1
    // 按键检测引脚配置 P10
    PUCON &= ~(0x01 << 0); // 上拉电阻 20K
    DDR1 |= 0x01 << 0;     // 输入模式
#else
    // 使用仿真板上的 P12 检测按键
    PUCON &= ~(0x01 << 2); // 上拉电阻 20K
    DDR1 |= (0x01 << 2);   // P12 输入模式
#endif

    if (1 == KEY_SCAN_PIN) {
        cur_key_id = KEY_ID_NONE;
    } else {
        // 按键按下
        cur_key_id = KEY_ID_VALID;

        // 有按键按下，清空自动关机、自动进入低功耗的计时
        pwr_off_cnt = 0;
        into_low_power_cnt = 0;
    }

    if (cur_key_id != filter_key_id) {
        // 如果有按键按下/松开
        filter_cnt = 0;
        filter_key_id = cur_key_id;
        return;
    }

    if (filter_cnt < KEY_FILTER_TIMES) {
        // 如果检测到相同的按键按下/松开
        // 防止计数溢出
        filter_cnt++;
        return;
    }

    // 滤波/消抖完成后，执行到这里
    if (last_key_id != cur_key_id) {
        // last_key_id 为有效键值，而 cur_key_id
        // 为无效键值，说明按键刚开始松开
        if (cur_key_id == KEY_ID_NONE) {
            // 开始计时，等待下次按键按下，最后判断有没有按键连击
            click_delay_cnt = 0;
        } else // cur_key_id 为有效键值，而 last_key_id
               // 为无效键值，说明按键刚按下
        {
            press_cnt = 0; // 重置按键按下时间计数
            click_cnt++;
        }

        if (click_cnt == 2) {
            // click_cnt == 2 // 双击
            // 客户说开关机的速度太慢，改成按键双击的第二下刚按下不久，就处理该事件
            flag_is_dev_working = ~flag_is_dev_working;

            if (flag_is_dev_working) {
                pen_pwr_on();
                // 让led指示灯立即更新状态
                led_sta_refresh_cnt = LED_STA_REFRESH_TIME;
            } else {
                // 通过按键关机之后，立即关闭指示灯，不等led刷新
                pen_pwr_off();
#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
                flag_led_1_on = 0;
                flag_led_2_on = 0;
                flag_led_3_on = 0;
                flag_led_4_on = 0;
#elif (PRODUCT_TYPE == PRODUCT_TYPE_P003_JW)
                flag_led_2_on = 0;
                flag_led_3_on = 0;
                flag_led_4_on = 0;
#endif
            }

            // 处理完成后，清空连击的次数
            click_cnt = 0;
        }
    } else {
        /*
            cur_key_id == last_key_id &&
            cur_key_id == KEY_ID_NONE，说明按键已经松开
        */
        if (cur_key_id == KEY_ID_NONE) { // 没有按键按下，说明松手
            if (click_cnt > 0) {
                if (click_delay_cnt >= KEY_CLICK_DELAY_TIME) {
                    // 如果超过了连击的等待时间，直接处理
                    click_cnt = 0; // 清空连击的次数
                } else {
                    if (click_delay_cnt < 255) {
                        // 防止计数溢出
                        click_delay_cnt++;
                    }
                }
            }
        }
        // cur_key_id == last_key_id && cur_key_id !=
        // KEY_ID_NONE，说明按键按下未松开
        else {
            // 如果按键按住不放
            if (press_cnt < 255) {
                press_cnt++;
            }
        }
    }

    last_key_id = cur_key_id;
}

#elif ((PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                               \
       (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
// 按键 单击 来进行开关机

// 按键检测，放在10ms定时器中断处理
void key_scan(void)
{
    if (flag_is_in_charging) {
        // 充电期间，不检测按键
        last_key_id = 0;
        press_cnt = 0;
        filter_cnt = 0;
        filter_key_id = 0;
        click_delay_cnt = 0;
        click_cnt = 0;
        cur_key_id = 0;
        return;
    }

    led_all_off();
#if 1
    // 按键检测引脚配置 P10
    PUCON &= ~(0x01 << 0); // 上拉电阻 20K
    DDR1 |= 0x01 << 0;     // 输入模式
#else
    // 使用仿真板上的 P12 检测按键
    PUCON &= ~(0x01 << 2); // 上拉电阻 20K
    DDR1 |= (0x01 << 2);   // P12 输入模式
#endif

    if (1 == KEY_SCAN_PIN) {
        cur_key_id = KEY_ID_NONE;
    } else {
        // 按键按下
        cur_key_id = KEY_ID_VALID;

        // 有按键按下，清空自动关机、自动进入低功耗的计时
        pwr_off_cnt = 0;
        into_low_power_cnt = 0;
    }

    if (cur_key_id != filter_key_id) {
        // 如果有按键按下/松开
        filter_cnt = 0;
        filter_key_id = cur_key_id;
        return;
    }

    if (filter_cnt < KEY_FILTER_TIMES) {
        // 如果检测到相同的按键按下/松开
        // 防止计数溢出
        filter_cnt++;
        return;
    }

    // 滤波/消抖完成后，执行到这里
    if (last_key_id != cur_key_id) {
        // last_key_id 为有效键值，而 cur_key_id
        // 为无效键值，说明按键刚开始松开
        if (cur_key_id == KEY_ID_NONE) {
            // 开始计时，等待下次按键按下，最后判断有没有按键连击
            click_delay_cnt = 0;
        } else // cur_key_id 为有效键值，而 last_key_id
               // 为无效键值，说明按键刚按下
        {
            press_cnt = 0; // 重置按键按下时间计数
            click_cnt++;
        }

        if (click_cnt == 1) {
            // click_cnt == 1 // 单击
            /*
                客户说开关机的速度太慢，
                改成按键刚按下，就处理该事件
            */
            flag_is_dev_working = ~flag_is_dev_working;

            if (flag_is_dev_working) {
                pen_pwr_on();
                // 让led指示灯立即更新状态
                led_sta_refresh_cnt = LED_STA_REFRESH_TIME;
            } else {
                // 通过按键关机之后，立即关闭指示灯，不等led刷新
                pen_pwr_off();
#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
                // 如果有四个指示灯

                flag_led_1_on = 0;
                flag_led_2_on = 0;
                flag_led_3_on = 0;
                flag_led_4_on = 0;
#elif 1
#error "PRODUCT_TYPE error"
                // #elif (PRODUCT_TYPE == 只有三个指示灯的设备对应的宏)

                // 如果只有三个指示灯

                flag_led_2_on = 0;
                flag_led_3_on = 0;
                flag_led_4_on = 0;
#endif
            }

            // 处理完成后，清空连击的次数
            click_cnt = 0;
        }
    } else {
        /*
            cur_key_id == last_key_id &&
            cur_key_id == KEY_ID_NONE，说明按键已经松开
        */
        if (cur_key_id == KEY_ID_NONE) { // 没有按键按下，说明松手
            if (click_cnt > 0) {
                if (click_delay_cnt >= KEY_CLICK_DELAY_TIME) {
                    // 如果超过了连击的等待时间，直接处理
                    click_cnt = 0; // 清空连击的次数
                } else {
                    if (click_delay_cnt < 255) {
                        // 防止计数溢出
                        click_delay_cnt++;
                    }
                }
            }
        } else {
            /*
                cur_key_id == last_key_id &&
                cur_key_id != KEY_ID_NONE，说明按键按下未松开
                按键按住不放
            */
            if (press_cnt < 255) {
                press_cnt++;
            }
        }
    }

    last_key_id = cur_key_id;
}
#else
#error "PRODUCT_TYPE error"
#endif

#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
// 有四个指示灯的设备

// 放在 100ms 的定时器中断内调用
void led_status_handle(void)
{
    led_sta_refresh_cnt++;
    if (led_sta_refresh_cnt >= LED_STA_REFRESH_TIME) {
        led_sta_refresh_cnt = 0;

        if (flag_is_in_charging) {
            // 充电时，控制指示灯跑马显示

            // 如果刚进入充电，执行跑马灯动画，到对应的电压后停止
            if (flag_is_charge_begin) {
                if (bat_lev < BAT_LEV_3V3) {
                    /*
                        电池电量 小于 BAT_LEV_3V3 ，退出刚开始充电的动画
                        准备让 第一格 指示灯闪烁
                    */
                    flag_is_charge_begin = 0;
                } else if (bat_lev >= BAT_LEV_3V3 && charge_anim_phase == 0) {
                    /*
                        电池电量 大于等于 BAT_LEV_3V3
                        让 第一格 指示灯常亮
                    */
                    flag_led_1_on = 1;
                    charge_anim_phase = 1;
                } else if (bat_lev >= BAT_LEV_3V6 && charge_anim_phase == 1) {
                    /*
                        电池电量 大于等于 BAT_LEV_3V6
                        让 第二格 指示灯常亮
                    */
                    flag_led_2_on = 1;
                    charge_anim_phase = 2;
                } else if (bat_lev >= BAT_LEV_4V0 && charge_anim_phase == 2) {
                    /*
                        电池电量 大于等于 BAT_LEV_4V0
                        让 第三格 指示灯常亮
                    */
                    flag_led_3_on = 1;
                    charge_anim_phase = 3;
                } else if (bat_lev >= BAT_LEV_4V2 && charge_anim_phase == 3) {
                    // 电池电量 大于等于 BAT_LEV_4V2
                    flag_led_4_on = 1;
                    charge_anim_phase = 4;
                } else {
                    flag_is_charge_begin = 0; // 退出刚开始充电的动画
                    /*
                        退出刚开始充电的动画之后，需要立即进入对应的处理
                        这里给计数值恢复成 500 ms
                    */
                    led_sta_refresh_cnt = LED_STA_REFRESH_TIME;
                }
            } else {
                // 如果不是刚进入充电，而是正在充电，根据电池电量档位来控制指示灯闪烁
                switch (bat_lev) {
                case BAT_LEV_4V2:
                case BAT_LEV_4V0: // 4.0V 以上，用第四格指示灯
                    flag_led_4_on = ~flag_led_4_on;
                    break;
                case BAT_LEV_3V6: // 3.6V ~ 4.0V，用第三格指示灯
                    flag_led_3_on = ~flag_led_3_on;
                    break;
                case BAT_LEV_3V3: // 3.3V ~ 3.6V，用第二格指示灯
                    flag_led_2_on = ~flag_led_2_on;
                    break;
                // default:
                case BAT_LEV_3V2:
                case BAT_LEV_3V0:
                case BAT_LEV_2V9: // 3.3V 以下的电压，用 第一格 指示灯
                    flag_led_1_on = ~flag_led_1_on;
                    break;
                }

                if (bat_lev >= BAT_LEV_3V3) {
                    flag_led_1_on = 1; // 指示灯常亮，不闪烁
                }

                if (bat_lev >= BAT_LEV_3V6) {
                    flag_led_2_on = 1; // 指示灯常亮，不闪烁
                }

                if (bat_lev >= BAT_LEV_4V0) {
                    flag_led_3_on = 1; // 指示灯常亮，不闪烁
                }
            }
        } else if (flag_is_dev_working) {
            // 放电时，控制指示灯常亮

            if (bat_lev < BAT_LEV_3V2) {
                // 低电量，指示灯闪烁
                flag_led_1_on = ~flag_led_1_on;
            }

            if (bat_lev >= BAT_LEV_3V2) {
                flag_led_1_on = 1;
            }

            if (bat_lev >= BAT_LEV_3V3) {
                flag_led_2_on = 1;
            }

            if (bat_lev >= BAT_LEV_3V6) {
                flag_led_3_on = 1;
            }

            if (bat_lev >= BAT_LEV_4V0) {
                flag_led_4_on = 1;
            }
        } else {
            // 不在充电，设备也没有在工作，关闭所有指示灯
            flag_led_1_on = 0;
            flag_led_2_on = 0;
            flag_led_3_on = 0;
            flag_led_4_on = 0;
        }
    }

    if (flag_is_in_charging && 0 == flag_is_charge_begin) {
        // 正在充电，并且已经过了充电的开始动画
        if (bat_lev >= BAT_LEV_4V2) {
            flag_led_4_on = 1;
        }

        if (bat_lev >= BAT_LEV_4V0) {
            if (charge_fully_cnt < CHARGE_FULL_ACCUMULATE_TIME) {
                charge_fully_cnt++;
            } else {
                flag_led_4_on = 1;
            }
        }
    }
}

#elif (PRODUCT_TYPE == PRODUCT_TYPE_P003_JW)
// 只有三个指示灯的设备

// 放在 100ms 的定时器中断内调用
void led_status_handle(void)
{
    led_sta_refresh_cnt++;
    if (led_sta_refresh_cnt >= LED_STA_REFRESH_TIME) {
        led_sta_refresh_cnt = 0;

        if (flag_is_in_charging) {
            // 充电时，控制指示灯跑马显示

            // 如果刚进入充电，执行跑马灯动画，到对应的电压后停止
            if (flag_is_charge_begin) {
                if (bat_lev < BAT_LEV_3V3) {
                    /*
                        电池电量 小于 BAT_LEV_3V3 ，退出刚开始充电的动画
                        准备让 第一格 指示灯闪烁
                    */
                    flag_is_charge_begin = 0;
                } else if (bat_lev >= BAT_LEV_3V3 && charge_anim_phase == 0) {
                    /*
                        电池电量 大于等于 BAT_LEV_3V3
                        让 第一格 指示灯常亮
                    */
                    flag_led_2_on = 1;
                    charge_anim_phase = 1;
                } else if (bat_lev >= BAT_LEV_3V6 && charge_anim_phase == 1) {
                    /*
                        电池电量 大于等于 BAT_LEV_3V6
                        让 第二格 指示灯常亮
                    */
                    flag_led_3_on = 1;
                    charge_anim_phase = 2;
                } else if (bat_lev >= BAT_LEV_4V0 && charge_anim_phase == 2) {
                    /*
                        电池电量 大于等于 BAT_LEV_4V0
                        让 第三格 指示灯常亮
                    */
                    flag_led_4_on = 1;
                    charge_anim_phase = 3;
                } else {
                    flag_is_charge_begin = 0; // 退出刚开始充电的动画
                    /*
                        退出刚开始充电的动画之后，需要立即进入对应的处理
                        这里给计数值恢复成 500 ms
                    */
                    led_sta_refresh_cnt = LED_STA_REFRESH_TIME;
                }
            } else {
                // 如果不是刚进入充电，而是正在充电，根据电池电量档位来控制指示灯闪烁
                switch (bat_lev) {
                case BAT_LEV_4V2:
                case BAT_LEV_4V0: // 4.0V 以上，用第四格指示灯
                    flag_led_4_on = ~flag_led_4_on;
                    break;
                case BAT_LEV_3V6: // 3.6V ~ 4.0V，用第 二 格指示灯
                    flag_led_3_on = ~flag_led_3_on;
                    break;
                case BAT_LEV_3V3: // 3.3V ~ 3.6V，
                case BAT_LEV_3V2:
                case BAT_LEV_3V0:
                case BAT_LEV_2V9: // 3.3V 以下的电压，用 第一格 指示灯
                    // flag_led_1_on = ~flag_led_1_on;
                    flag_led_2_on = ~flag_led_2_on;
                    break;
                }

                if (bat_lev >= BAT_LEV_3V6) {
                    flag_led_2_on = 1; // 指示灯常亮，不闪烁
                }

                if (bat_lev >= BAT_LEV_4V0) {
                    flag_led_3_on = 1; // 指示灯常亮，不闪烁
                }
            }
        } else if (flag_is_dev_working) {
            // 放电时，控制指示灯常亮

            if (bat_lev < BAT_LEV_3V3) {
                // 小于 3.3V 的电量，指示灯闪烁
                flag_led_2_on = ~flag_led_2_on;
            }

            if (bat_lev >= BAT_LEV_3V3) {
                flag_led_2_on = 1;
            }

            if (bat_lev >= BAT_LEV_3V6) {
                flag_led_3_on = 1;
            }

            if (bat_lev >= BAT_LEV_4V0) {
                flag_led_4_on = 1;
            }

        } else {
            // 不在充电，设备也没有在工作，关闭所有指示灯
            flag_led_2_on = 0;
            flag_led_3_on = 0;
            flag_led_4_on = 0;
        }
    }

    if (flag_is_in_charging && 0 == flag_is_charge_begin) {
        // 正在充电，并且已经过了充电的开始动画
        if (bat_lev >= BAT_LEV_4V2) {
            flag_led_4_on = 1;
        }

        if (bat_lev >= BAT_LEV_4V0) {
            if (charge_fully_cnt < CHARGE_FULL_ACCUMULATE_TIME) {
                charge_fully_cnt++;
            } else {
                flag_led_4_on = 1;
            }
        }
    }
}

#endif

#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
// 有四个指示灯的设备

// 根据电池电量和设备状态，来控制电池电量指示灯
// 目前暂定在 100us 定时器中断里面调用
void led_refresh(void)
{
    static volatile u8 sta = 0;

    if (0 == flag_is_led_show_enable) {
        return;
    }

    led_all_off();

    switch (sta) {
    case 0:
        if (flag_led_1_on) {
            led1_on();
        }
        // sta = 1;
        break;
    case 1:
        if (flag_led_2_on) {
            led2_on();
        }
        // sta = 2;
        break;
    case 2:
        if (flag_led_3_on) {
            led3_on();
        }
        // sta = 3;
        break;
    case 3:
        if (flag_led_4_on) {
            led4_on();
        }
        // sta = 0;
        break;

    default:
        break;
    }

    sta++;
    if (sta >= 4) {
        sta = 0;
    }
}
#elif (PRODUCT_TYPE == PRODUCT_TYPE_P003_JW)
// 只有三个指示灯的设备

// 根据电池电量和设备状态，来控制电池电量指示灯
// 目前暂定在 100us 定时器中断里面调用
void led_refresh(void)
{
    static volatile u8 sta = 0;

    if (0 == flag_is_led_show_enable) {
        return;
    }

    led_all_off();

    switch (sta) {
    case 0:
        if (flag_led_2_on) {
            led2_on();
        }
        // sta = 1;
        break;
    case 1:
        if (flag_led_3_on) {
            led3_on();
        }
        // sta = 2;
        break;
    case 2:
        if (flag_led_4_on) {
            led4_on();
        }
        // sta = 0;
        break;
    default:
        break;
    }

    sta++;
    if (sta >= 3) {
        sta = 0;
    }
}
#endif

void main(void)
{
    Sys_Init();
    user_init();

    into_low_power_cnt = (u8)((u16)2000 / 10); // 确保一上电就进入低功耗

    while (1) {
#if 1
        /*
            如果不在充电，并且到了关机的时间

            如果不在充电并且设备不在运行，2s后进入低功耗
            改成直接进低功耗
        */
        if ((flag_is_dev_working && pwr_off_cnt >= (DEV_AUTO_SHUTDOWN_TIME)) ||
            into_low_power_cnt >= (u8)((u16)2000 / 10)) {
        label_low_power_in: // 标签，进入低功耗

            GIE = 0; // 关闭总中断

            // 关闭定时器：
            T0IE = 0;  // 屏蔽 timer0 中断
            T1IE = 0;  // 屏蔽 timer1 中断
            T1EN = 0;  // 不使能定时器
            LVDEN = 0; // 关闭 lvd

            // 所有引脚配置为输入模式、关闭上下拉
            PUCON = 0xFF; // 0:Effective 1:invalid
            PDCON = 0xFF; // 0:Effective 1:invalid
            DDR1 = 0xFF;  // 1:input 0:output

            PUCON &= ~((0x01 << 0) | // 上拉 20K ， P10(按键检测脚)
                                     // LED控制 A端和B端，打开上拉
                       (0x01 << 1) | (0x01 << 2));

            // 使能键盘中断
            KBIF = 0; // 清除中断标志
            KBIE = 1; // 使能键盘中断

            // P10(按键检测脚) 、 P13(充电检测脚) 打开键盘中断
            P1KBCR |= (0x01 << 0) | (0x01 << 3);

            // =================================================================
            // 进入低功耗
            Stop();
            Nop();
            Nop();
            // =================================================================
            // 唤醒之后，关闭键盘中断
            P1KBCR = 0; // 关闭所有键盘中断
            KBIE = 0;   // 关闭键盘中断
            KBIF = 0;   // 清除 键盘中断 标志
            /*
                LED控制 A端和B端，关闭上拉
                后面有调用 IO_Init() 关闭了所有上下拉，这里可以省略
            */
            // PUCON |= (0x01 << 1 | 0x01 << 2);

            Sys_Init();
            user_init();

            if (CHARGE_PIN == 0) {
                // 没有在充电，可能是按键唤醒，这里给变量最大值作为初始值
                tmp_bat_lev = BAT_LEV_4V2; // 唤醒后，给一个默认值
                bat_lev = BAT_LEV_4V2;
            } else {
                // 在充电，可能是充电唤醒，这里给变量最小值作为初始值
                tmp_bat_lev = BAT_LEV_2V9;
                bat_lev = BAT_LEV_2V9;
            }

            /*
                在 timer0 中断内调用了 lvd_scan()，这里要等待一段时间，
                等 tmp_bat_lev 的值更新
            */
            delay_ms(10);
            // 强制更新 bat_lev
            // ，防止开机后又立刻刷新了电量，导致显示的电池电量格数变化
            bat_lev = tmp_bat_lev;
            flag_is_led_show_enable = 1; // 确保电池电量更新后，再使能led显示

            // 没有在充电并且低电量，重新回到低功耗
            if (CHARGE_PIN == 0 && bat_lev < BAT_LEV_3V0) {
                goto label_low_power_in;
            }
        }
#endif
    }
}

/************************************************
;  *    @Function Name       : Interrupt
;  *    @Description         : The interrupt function
;  *    @IN_Parameter          	 :
;  *    @Return parameter      	:
;  ***********************************************/
void int_isr(void) __interrupt
{
    __asm;
    movra _abuf;
    swapar _STATUS;
    movra _statusbuf;
    __endasm;

    if ((T0IF) && (T0IE)) {
        T0IF = 0;
        T0CNT = (u8)(255 - 125); // T0 目前1ms产生一次中断

        {
            static volatile u8 cnt_1ms = 0;
            cnt_1ms++;
            if (cnt_1ms >= 10) {
                cnt_1ms = 0;
                // 10ms进入一次

                {
                    static volatile u8 cnt_10ms = 0;
                    cnt_10ms++;
                    if (cnt_10ms >= 10) {
                        cnt_10ms = 0;
                        // 100 ms 进入一次

                        led_status_handle();
                        if (flag_is_dev_working) {
                            /*
                                防止计数溢出

                                现在为了节省程序空间，省略了判断条件，
                                如果计数条件满足，会在主循环进入低功耗
                            */
                            // if (pwr_off_cnt < DEV_AUTO_SHUTDOWN_TIME) {
                            pwr_off_cnt++;
                            // }
                        } else {
                            // 不在工作，清空计数
                            pwr_off_cnt = 0;
                        }
                    }
                }

                /*
                    充电检测
                    REVIEW 先执行充电检测，再执行按键扫描，避免刚拔出充电器，
                    而检测不到按键事件
                */
                {
                    // RAM空间不够，省略了充电检测的消抖和滤波操作
                    static volatile u8 is_not_charge_cnt = 0;
                    /*
                        一旦有充电信号，就直接进入充电判断逻辑
                        断开充电时，才加入滤波，避免误判
                    */
                    if (CHARGE_PIN) {
                        is_not_charge_cnt = 0; // 只要有充电信号，就清空计数
                        if (0 == flag_is_in_charging) {
                            /*
                                刚进入充电，需要将记录的电池电量改为最低档，
                                让它之后升上去
                            */
                            tmp_bat_lev = BAT_LEV_2V9;
                            bat_lev = BAT_LEV_2V9;
                            pen_pwr_off(); // 断开笔头的供电
                                           // 关闭所有指示灯，准备充电动画:

#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS))
                            flag_led_1_on = 0;
                            flag_led_2_on = 0;
                            flag_led_3_on = 0;
                            flag_led_4_on = 0;
#elif (PRODUCT_TYPE == PRODUCT_TYPE_P003_JW)
                            flag_led_2_on = 0;
                            flag_led_3_on = 0;
                            flag_led_4_on = 0;
#endif
                            flag_is_dev_working = 0; // 表示设备关闭
                            // 表示刚进入充电
                            flag_is_charge_begin = 1;
                            charge_anim_phase = 0;
                            // 刚进入充电，清空充满电的计数值
                            charge_fully_cnt = 0;
                            flag_is_in_charging = 1;
                        }
                    } else {
                        is_not_charge_cnt++;
                        if (is_not_charge_cnt >= (200 / 10)) {
                            is_not_charge_cnt = 0;
                            flag_is_in_charging = 0;
                        }
                    }
                }

                key_scan();

                // 低电量关机
                if (flag_is_dev_working && bat_lev == BAT_LEV_2V9) {
                    pen_pwr_off();           // 断开笔头的供电
                    led_all_off();           // 关闭所有指示灯
                    flag_is_dev_working = 0; // 表示设备关闭
                }

                // 如果不在充电、设备也没有在工作
                if ((CHARGE_PIN == 0) && (flag_is_dev_working == 0)) {

                    // if (into_low_power_cnt < 255) {
                    into_low_power_cnt++;
                    // }
                } else {
                    into_low_power_cnt = 0;
                }
            }
        }

        // USER_TO_DO 如果还有多的程序空间，要给这里加上滤波
        {
            if (flag_is_dev_working) {
                // 正在放电，电量只能减小，不能增大
                if (bat_lev >= tmp_bat_lev) {
                    bat_lev = tmp_bat_lev;
                }
            } else {
                // 正在充电，电量只能增大，不能减小
                if (bat_lev <= tmp_bat_lev) {
                    bat_lev = tmp_bat_lev;
                }
            }
        }
    }

    // 100us 定时器中断
    if ((T1IF) && (T1IE)) {
        T1IF = 0;

        led_refresh();

        {
            static volatile u8 cnt = 0;
            cnt++;
            if (cnt >= 3) {
                cnt = 0;
                lvd_scan();
            }
        }

#if (PRODUCT_TYPE == PRODUCT_TYPE_P004_DS)
        如果检测到的振动传感器传来的信号，当前检测脚的电平跟上次的不一样
        if ((VIBRATION_SENSOR_PIN && last_vibration_sensor_lev == 0) ||
            (VIBRATION_SENSOR_PIN == 0 && last_vibration_sensor_lev)) {
            // 清空关机的计时
            pwr_off_cnt = 0;

            if (VIBRATION_SENSOR_PIN) {
                last_vibration_sensor_lev = 1;
            } else {
                last_vibration_sensor_lev = 0;
            }
        }
#endif
    }

    __asm;
    swapar _statusbuf;
    movra _STATUS;
    swapr _abuf;
    swapar _abuf;
    __endasm;
}

/**************************** end of file
 * *********************************************/
