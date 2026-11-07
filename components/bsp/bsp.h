#pragma once

#include <stdbool.h>

#include "servo_drv.h"
#include "led_drv.h"
#include "key_drv.h"

// ========== 板级层 ========== //
//
// 职责：
//   1. 集中所有引脚和器件参数（见 bsp_config.h）
//   2. 按正确顺序初始化各驱动（bsp_init）
//   3. 持有需要多实例的器件句柄（g_servo / g_ledX / g_key）
//
// 不负责：
//   ✗ 业务逻辑（什么时候转舵机、显示什么）
//   ✗ 驱动实现（在 drv 里）
//
// 用法：
//    void app_main(void)
//    {
//        bsp_init();                  // OLED / 电位器 / 舵机 / LED
//        bsp_key_init(key_cb, NULL);  // 按键：BSP 给引脚，app 给回调
//        app_start();
//    }
//
// 注意：OLED 和电位器是【单实例】驱动，内部自己管实例，
//       所以这里不暴露它们，直接调 drv_oled_* / drv_pot_* 即可。

// 初始化不需要业务参数的器件
// OLED / 电位器 / 舵机 / LED
// 返回 true = 全部成功；false = 有器件失败（其余仍然可用）
bool bsp_init(void);

// 初始化按键（单独一个函数，因为需要业务回调）
// cb  : 按键事件回调
// arg : 回调时原样传回
bool bsp_key_init(drv_key_cb_t cb, void *arg);

// ---------- 暴露给 app 的实例 ---------- //
// 多实例驱动必须暴露，否则 app 拿不到句柄
extern drv_servo_t   g_servo;
/*extern drv_led_t g_led1;
extern drv_led_t g_led2;
extern drv_led_t g_led3;
extern drv_key_t g_key;*/



