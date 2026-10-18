#pragma once

#include <stdint.h>
#include <stdbool.h>

/** LED 驱动：把 GPIO 输出抽象成 开 / 关 / 翻转，并记住当前状态
 *
 *  ==================== 怎么用 ====================
 *
 *    static drv_led_t s_led = {0};        // 零初始化，且必须是全局或 static 变量
 *
 *    drv_led_cfg_t cfg = { .gpio_num = 2, .level_active = true };
 *    drv_led_init(&s_led, &cfg);          // 初始化完成后默认是【灭】
 *
 *    drv_led_on(&s_led);
 *    drv_led_off(&s_led);
 *    drv_led_toggle(&s_led);
 *
 *  ==================== level_active 由接线决定 ====================
 *
 *    GPIO ──电阻── LED长脚(+) ── LED短脚(-) ── GND    → true   （高电平点亮）
 *    3V3 ── LED长脚(+) ── LED短脚(-) ──电阻── GPIO    → false  （低电平点亮）
 *
 *    填反了的现象是"该亮的灭、该灭的亮"，不是驱动坏了。
 *    另外：LED 长脚是阳极(+)，短脚是阴极(-)。接反了不亮，但不会烧坏。
 *
 *  ==================== 硬件注意 ====================
 *
 *    1. 一定要串电阻（470Ω 左右）。直连会烧 GPIO，也会烧 LED。
 *    2. 别用 GPIO6~11 —— 那是接 flash 的，一动就崩。
 *    3. GPIO0 / 2 / 12 / 15 是启动脚，上电那一刻的电平会影响启动模式，
 *       能避开就避开。安全的选择：4、5、13、14、16~19、21~23、25~27、32、33。
 *
 *  ==================== 一条重要约定 ====================
 *
 *    不要绕过本驱动、直接调 hal_gpio_set_level() 操作同一个引脚。
 *    驱动内部记了一份 state（开/关），toggle 靠它判断方向。
 *    如果别处改了电平却没更新 state，两者就错开了，
 *    之后 toggle 会一直翻错方向，而且不会自己恢复。
 */

// ========== LED STATE ========== // 
typedef enum
{
    LED_DRV_OFF = 0,  // 关闭
    LED_DRV_ON  = 1,  // 开启
    LED_DRV_MAX,
} drv_led_state_t;

// ========== LED CONFIG ========== //
typedef struct
{
    uint8_t gpio_num;  // 引脚号
    bool level_active; // 激活电平
} drv_led_cfg_t;

// ========== LED DRIVER ========== //
typedef struct
{
    drv_led_cfg_t cfg;      // 配置
    drv_led_state_t state;  // 状态
} drv_led_t;

bool drv_led_init(drv_led_t* led, drv_led_cfg_t* cfg);
void drv_led_on(drv_led_t* led);
void drv_led_off(drv_led_t* led);
void drv_led_toggle(drv_led_t* led);

