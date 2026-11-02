/******************************************************************************
;  *   	@型号				  : MC30P6280
;  *   	@创建日期             : 2021.08.04
;  *   	@公司/作者			  : SINOMCU-FAE
;  *   	@晟矽微技术支持       : 2048615934
;  *   	@晟矽微官网           : http://www.sinomcu.com/
;  *   	@版权                 : 2021 SINOMCU公司版权所有.
;  *---------------------- 建议 ---------------------------------
;  *   				变量定义时使用全局变量
******************************************************************************/
#ifndef USER
#define USER

#include "mc30-common.h"
#include "mc30P6280.h"

#include "user_config.h"

#define u8       unsigned char
#define u16      unsigned int
#define u32      unsigned long int
#define uint8_t  unsigned char
#define uint16_t unsigned int
#define uint32_t unsigned long int

#define DEF_SET_BIT0 0x01
#define DEF_SET_BIT1 0x02
#define DEF_SET_BIT2 0x04
#define DEF_SET_BIT3 0x08
#define DEF_SET_BIT4 0x10
#define DEF_SET_BIT5 0x20
#define DEF_SET_BIT6 0x40
#define DEF_SET_BIT7 0x80

#define DEF_CLR_BIT0 0xFE
#define DEF_CLR_BIT1 0xFD
#define DEF_CLR_BIT2 0xFB
#define DEF_CLR_BIT3 0xF7
#define DEF_CLR_BIT4 0xEF
#define DEF_CLR_BIT5 0xDF
#define DEF_CLR_BIT6 0xBF
#define DEF_CLR_BIT7 0x7F

#define ENABLE  1
#define DISABLE 0

#define FAIL 1
#define PASS 0

#define USER_DEBUG_ENABLE 0

#if ((PRODUCT_TYPE == PRODUCT_TYPE_P004_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P003_JW) ||                                 \
     (PRODUCT_TYPE == PRODUCT_TYPE_P004_DW))

// 设备自动关机的时间，单位：100ms
#define DEV_AUTO_SHUTDOWN_TIME ((u16)((u32)50 * 60 * 1000 / 100))
#elif ((PRODUCT_TYPE == PRODUCT_TYPE_P004_DS) ||                               \
       (PRODUCT_TYPE == PRODUCT_TYPE_P003_JS))
// 设备自动关机的时间，单位：100ms
#define DEV_AUTO_SHUTDOWN_TIME ((u16)((u32)5 * 60 * 1000 / 100))
#endif

// 充电电压达到 4.0V 后，累计多少时间后判定为充满电，单位：100ms
#define CHARGE_FULL_ACCUMULATE_TIME ((u16)((u32)13 * 60 * 1000 / 100))

// 指示灯状态的刷新间隔，单位：100ms
#define LED_STA_REFRESH_TIME ((u8)(500 / 100))

// 按键检测引脚
#if 1
#define KEY_SCAN_PIN P10D
#else
#define KEY_SCAN_PIN P12D // USER_TO_DO 测试时使用
#endif
// 振动传感器检测脚
#define VIBRATION_SENSOR_PIN P15D
// 充电检测脚
#define CHARGE_PIN P13D

// 定义LVD各个电压检测阈值配置：
// 调用下面这些配置之前，需要先给对应的寄存器全部清零：
#define MCR_LVD_CFG_ALL ((u8)(0x01 << 4 | 0x01 << 3 | 0x01 << 2 | 0x01 << 1))
#define MCR_LVD_CFG_4V2 ((u8)(0x01 << 4 | 0x01 << 3 | 0x01 << 2 | 0x01 << 1))
#define MCR_LVD_CFG_4V0 ((u8)(0x01 << 4 | 0x01 << 3 | 0x01 << 2))
#define MCR_LVD_CFG_3V6 ((u8)(0x01 << 4 | 0x01 << 3 | 0x01 << 1))
#define MCR_LVD_CFG_3V3 ((u8)(0x01 << 4 | 0x01 << 3))
#define MCR_LVD_CFG_3V2 ((u8)(0x01 << 4 | 0x01 << 2 | 0x01 << 1))
#define MCR_LVD_CFG_3V0 ((u8)(0x01 << 4 | 0x01 << 2))
#define MCR_LVD_CFG_2V9 ((u8)(0x01 << 4 | 0x01 << 1))

// 定义在lvd扫描时,需要切换到的各个配置状态:
enum
{
    LVD_LEV_NONE = 0,

    LVD_LEV_4V2_SWITCHING,
    LVD_LEV_4V2,

    LVD_LEV_4V0_SWITCHING,
    LVD_LEV_4V0,

    LVD_LEV_3V6_SWITCHING,
    LVD_LEV_3V6,

    LVD_LEV_3V3_SWITCHING,
    LVD_LEV_3V3,

    LVD_LEV_3V2_SWITCHING,
    LVD_LEV_3V2,

    LVD_LEV_3V0_SWITCHING,
    LVD_LEV_3V0,

    // 正在切换中(手册说,切换LVD档位后,至少等200us，LVD输出才稳定)
    LVD_LEV_2V9_SWITCHING,
    LVD_LEV_2V9, // 切换完成
};

// 定义电池电量的档位:
enum
{
    BAT_LEV_2V9 = 0, // 关机电压
    BAT_LEV_3V0,     //
    BAT_LEV_3V2,     // 低电量
    BAT_LEV_3V3,     //
    BAT_LEV_3V6,     // 表示当前电压至少大于3.6V
    BAT_LEV_4V0,     // 表示当前电压至少大于4.0V
    BAT_LEV_4V2,     // 表示当前电压至少大于4.2V
};
typedef u8 bat_lev_t;

// 如果只消抖2次，会过滤不掉抖动
// 按键消抖次数 (消抖时间 == 消抖次数 * 按键扫描时间)
#define KEY_FILTER_TIMES (3)
// 按键松开后，等待连击的延时，单位：10 ms
#define KEY_CLICK_DELAY_TIME ((u16)300 / 10)
// 定义按键键值
enum
{
    KEY_ID_NONE = 0x00,
    KEY_ID_VALID,
};

// 定义设备的状态
enum
{
    DEV_STA_IDLE = 0,
    DEV_STA_WORKING,
};
typedef u8 dev_sta_t;

//===============Field Protection Variables===============
extern volatile u8 abuf;
extern volatile u8 statusbuf;

//===============IO Define===============

//===============Global Variable===============

//===============Global Function===============
void C_RAM(void);
void IO_Init(void);
void Sys_Init(void);

void user_init(void);

void led_all_off(void);
void led1_on(void);
void led2_on(void);
void led3_on(void);
void led4_on(void);
void led_status_handle(void);
void led_refresh(void);

void key_scan(void);
void lvd_scan(void);

//===============Define  Flag===============
typedef union
{
    unsigned char byte;
    struct
    {
        u8 bit0 : 1;
        u8 bit1 : 1;
        u8 bit2 : 1;
        u8 bit3 : 1;
        u8 bit4 : 1;
        u8 bit5 : 1;
        u8 bit6 : 1;
        u8 bit7 : 1;
    } bits;
} bit_flag_t;
// example:
// #define  	FLAG_TIMER0_500ms  	flag1.bits.bit0	   	 // 标志位
extern volatile bit_flag_t flag1;
extern volatile bit_flag_t flag2;

// 是否正在充电
#define flag_is_in_charging flag1.bits.bit0
// 是否刚进入充电
#define flag_is_charge_begin flag1.bits.bit1
// 控制标志位，是否点亮led：
#define flag_led_1_on flag1.bits.bit2
#define flag_led_2_on flag1.bits.bit3
#define flag_led_3_on flag1.bits.bit4
#define flag_led_4_on flag1.bits.bit5
// 设备是否运转（ USER_TO_DO 充电时，需要清空该标志位）
#define flag_is_dev_working flag1.bits.bit6

// 上一次从传感器检测脚读取到的电平
#define last_vibration_sensor_lev flag1.bits.bit7

// 是否使能led指示灯显示
#define flag_is_led_show_enable flag2.bits.bit0

#include "timer0.h"
#include "timer1.h"

#endif

/**************************** end of file
 * *********************************************/