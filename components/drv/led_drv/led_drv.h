#pragma once

#include <stdint.h>
#include <stdbool.h>

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

