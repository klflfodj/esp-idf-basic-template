#pragma once

#include <stdint.h>
#include <stdbool.h>

// 舵机配置结构体
typedef struct
{
    int      pin;        // PWM 引脚
    uint16_t min_us;     // 0° 对应的脉宽，例如 500
    uint16_t max_us;     // 180° 对应的脉宽，例如 2500
    uint16_t home_ms;    // 归零时长(ms)，0 = 不平滑
} drv_servo_cfg_t;

// 舵机结构体
typedef struct
{
    int      pin;        // PWM 引脚
    uint16_t min_us;     // 0° 的脉宽
    uint16_t max_us;     // 180° 的脉宽
    uint8_t  angle;      // 当前角度 0~180
    bool     inited;     // 是否已初始化
} drv_servo_t;

bool drv_servo_init(drv_servo_t *s, const drv_servo_cfg_t *cfg);
bool drv_servo_set_angle(drv_servo_t *s, uint8_t angle);
bool drv_servo_move(drv_servo_t *s, uint8_t target, uint16_t duration_ms);
bool drv_servo_set_us(drv_servo_t *s, uint16_t us);
bool drv_servo_get_angle(drv_servo_t *s, uint8_t *out_angle);

