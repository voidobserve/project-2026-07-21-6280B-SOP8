#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

// 定义各个产品的类型，这里只需要数值不重复即可
#define PRODUCT_TYPE_P004_JW 0 // 双击 开关机、定时50分钟关机
#define PRODUCT_TYPE_P003_JW 1 // 双击 开关机、定时50分钟关机

#define PRODUCT_TYPE_P004_DW 2 // 单击来开关机、定时50分钟关机
// 单击来开关机、定时5分钟关机、带振动检测（有振动清空定时）
#define PRODUCT_TYPE_P004_DS 3

// 实际使用到的产品类型
#define PRODUCT_TYPE PRODUCT_TYPE_P004_DS

#endif
