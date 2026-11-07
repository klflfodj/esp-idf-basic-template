#pragma once

#include <stdint.h>
#include <stdbool.h>

// ========== 舵机驱动 ========== //
// 硬件：普通舵机（SG90 / MG995 等），3 根线（电源 / GND / 信号）
//       电源必须独立 5V，不要用 ESP32 的 3.3V
//       舵机 GND 和 ESP32 GND 必须共地，否则信号没有参考电平
//       建议在舵机电源并一个 100~470uF 电容，吸收启动电流
//
// 控制：50Hz PWM，脉宽 min_us ~ max_us 对应 0° ~ 180°
//       内部走 pwm_hal（万分比），50Hz 下 1 个单位 = 2us
//
// 用法：
//    static servo_t s_servo = {0};
//
//    servo_cfg_t cfg =
//    {
//        .pin     = 13,
//        .min_us  = 500,      // 0° 的脉宽
//        .max_us  = 2500,     // 180° 的脉宽
//        .home_ms = 500,      // 归零时长(ms)，0 = 不平滑直接到 0°
//    };
//    drv_servo_init(&s_servo, &cfg);
//
//    drv_servo_set_angle(&s_servo, 90);       // 立刻到 90°
//    drv_servo_move(&s_servo, 0, 1000);       // 1 秒内平滑转到 0°
//
// 注意：
//   1. init 会阻塞 home_ms（默认 500ms），因为要先到中位再平滑归零。
//   2. init 的第一帧给的是 90°（中位），不是 0°。
//      原因：舵机上电时不知道自己在哪，给中位最坏误差 90°，
//            给 0° 最坏误差 180°。这一下的"冲"无法避免。
//   3. 最多能接 4 个舵机（pwm_hal 的定时器上限）。
//   4. servo_t 必须是 static 或全局，生命周期要够长。
//   5. drv_servo_move() 是阻塞的，会占住当前任务，别在 1kHz 循环里调。

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

