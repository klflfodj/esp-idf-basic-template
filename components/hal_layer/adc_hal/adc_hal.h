#pragma once

#include <stdint.h>
#include <stdbool.h>

/** ADC HAL：对外只暴露"引脚号"，量程用 hal_adc_range_t 选
 *
 *  怎么用：
 *    hal_adc_init(35, HAL_ADC_RANGE_3_1V);   // GPIO35 = ADC1_CH7
 *    int mv = 0;
 *    if (hal_adc_read_mv(35, &mv)) { ... }
 *
 *  必须知道：
 *  1. 只用 ADC1（GPIO32~39）。ESP32 的 ADC2 和 WiFi 共用，WiFi 一开就读不出来。
 *  2. 量程要选"刚好够用"的：选太大分辨率浪费，选太小会饱和（读数卡在上限，不报错）。
 *  3. 12 位 → raw 范围 0~4095，它只是刻度数，不是电压。
 *  4. ESP32 精度一般（±50mV 量级），别当精密仪表用。
 */

// ADC 量程
typedef enum
{
    HAL_ADC_RANGE_1_1V   = 0,  // 0 ~ 1.1V，不分压
    HAL_ADC_RANGE_1_5V = 1,  // 0 ~ 1.5V，2.5dB分压
    HAL_ADC_RANGE_2_2V   = 2,  // 0 ~ 2.2V，6dB分压
    HAL_ADC_RANGE_3_1V  = 3,  // 0 ~ 3.1V，（3V3 系统用这个）12dB分压
    HAL_ADC_RANGE_MAX,
} hal_adc_range_t;  // ADC 量程

bool hal_adc_init(int gpio_num, hal_adc_range_t range);
bool hal_adc_read_raw(int gpio_num, int* out_raw);
bool hal_adc_read_mv(int gpio_num, int* out_mv);

