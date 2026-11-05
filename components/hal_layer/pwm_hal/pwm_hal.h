#pragma once

#include <stdint.h>
#include <stdbool.h>

// 占空比满量程（万分比）：10000 = 100%
#define HAL_PWM_DUTY_MAX       10000

/** PWM HAL（基于 LEDC）：一个引脚一路，可固定用也可随时变频变占空比
 *
 *  ==================== 怎么用 ====================
 *
 *    // 【固定 PWM】一次配好，之后不管
 *    hal_pwm_timer_cfg_t cfg = { .pin = 2, .freq_hz = 5000, .duty = 5000 };
 *    hal_pwm_init(&cfg);                  // GPIO2, 5kHz, 50%
 *
 *    // 【动态 PWM】
 *    hal_pwm_set_freq(2, 1000);           // 改成 1kHz
 *    hal_pwm_set_duty(2, 2000);           // 改成 20%
 *    hal_pwm_update(2, 1000, 2000);       // 两个一起改（推荐）
 *
 *    // 【查询实时状态】
 *    hal_pwm_info_t info;
 *    if (hal_pwm_get_info(2, &info))
 *    {
 *        printf("pin=%d freq=%u duty=%u.%02u%%\n",
 *               info.pin, (unsigned)info.freq_hz,
 *               (unsigned)(info.duty / 100),
 *               (unsigned)(info.duty % 100));
 *    }
 *
 *    hal_pwm_stop(2);                     // 停止并释放，引脚可另作他用
 *
 *  ==================== 必须知道的几条 ====================
 *
 *   1. 【最多 4 路】ESP32 的 LEDC 只有 4 个定时器。本驱动让每路独占一个定时器，
 *      这样每路都能独立变频。要更多路且频率相同，才需要共用定时器（本驱动没做）。
 *
 *   2. 占空比用【万分比 0~10000】，不是 0~100：
 *        10000 = 100%，5000 = 50%，1 = 0.01%
 *      用整数万分比是为了舵机和 LED 调光都够细，又避免 float。
 *
 *   3. 频率范围 1 Hz ~ 1 MHz。实际频率可能和你要的略有偏差（分频是整数），
 *      所以 hal_pwm_get_info() 返回的是【硬件真实频率】，不是你要的那个。
 *
 *   4. 分辨率是自动选的：频率越高，能用的占空比精度越低。
 *        5kHz  → 13 位（粗）
 *        1kHz  → 16 位
 *        50Hz  → 20 位（舵机够用）
 *      你不用管，驱动会挑当前频率下最高的那档。
 *
 *   5. 引脚不能用 6~11（flash）。34~39 只能输入，也不能输出 PWM。
 *
 *   6. 常见参数（万分比）：
 *        LED 调光      1000~5000 Hz，占空比 0~10000
 *        无源蜂鸣器    2000~5000 Hz，占空比 5000（50% 最响）
 *        舵机          50 Hz，占空比约 250~1250（0.5~2.5ms 对应 0~180°）
 */

// PWM 信息结构体，用于存储 PWM 配置信息
typedef struct
{
    int      pin;           // PWM 引脚编号
    uint8_t  timer;         // PWM 定时器编号
    uint8_t  channel;       // PWM 通道编号
    uint32_t freq_hz;       // PWM 频率，单位：兹
    uint32_t duty;          // PWM 职空比，单位：万分比
    uint8_t  resolution;    // PWM 分辨率，单位：位
    bool     used;          // PWM 是否被使用
} hal_pwm_info_t;  

typedef struct
{
    int      pin;           // PWM 引脚编号
    uint32_t freq_hz;       // PWM 频率，单位：兹
    uint32_t duty;          // PWM 职空比，单位：万分比
} hal_pwm_timer_cfg_t;

bool hal_pwm_init(const hal_pwm_timer_cfg_t *cfg);
bool hal_pwm_set_freq(int pin, uint32_t freq_hz);
bool hal_pwm_set_duty(int pin, uint32_t duty);
bool hal_pwm_update(int pin, uint32_t freq_hz, uint32_t duty);
bool hal_pwm_stop(int pin);
bool hal_pwm_get_info(int pin, hal_pwm_info_t *out);
