#pragma once

#include <stdint.h>
#include <stdbool.h>

// ========== 电位器驱动 ========== //
// 硬件：旋转电位器，3 个脚（GND / 抽头 / 3.3V）
// 接线：抽头必须接 ADC1 引脚（GPIO32~39），ADC2 和 WiFi 冲突不能用
//       两端接反了旋转方向会反过来，对调 GND 和 3.3V 即可
//
// 用法：
//    drv_pot_cfg_t cfg = { .gpio_num = 35 };
//    drv_pot_init(&cfg);
//
//    // 没校准前 permille 读不出来，要先校准
//    drv_pot_set_range(60, 4030);
//
//    // 之后正常用
//    uint16_t p;
//    if (drv_pot_read_permille(&p))
//    {
//        // p: 0 ~ 1000
//    }
//
// 注意：
//   1. 本驱动不保存校准值，掉电丢失。app 负责记 raw 的极值，
//      校准结束时调 drv_pot_set_range() 交进来。
//   2. 未校准时 drv_pot_read_permille() 返回 false，且不会打印任何东西
//      （主循环里调用不会刷屏）。想知道状态用 drv_pot_is_ready()。
//   3. 每次 drv_pot_read_raw() 内部采 16 次求平均，耗时约 0.6ms。

// 电位器配置结构体
typedef struct
{
    int gpio_num;    // ADC 引脚
} drv_pot_cfg_t;

bool drv_pot_init(const drv_pot_cfg_t *cfg);
bool drv_pot_is_ready(void);
uint16_t drv_pot_read_raw(void);
bool drv_pot_read_permille(uint16_t *out_permille);
bool drv_pot_set_range(uint16_t min_raw, uint16_t max_raw); 
bool drv_pot_get_range(uint16_t *min_raw, uint16_t *max_raw);




