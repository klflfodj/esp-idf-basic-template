#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** ADC HAL：把 ESP-IDF 的 ADC 包成"给个引脚号就能用"的接口
 *
 *  ==================== 怎么用 ====================
 *
 *   ★ 两种模式，在初始化时二选一，运行中不支持切换。
 *
 *   【方式一】单次读取（不用 DMA）—— 适合"隔一会儿读一下电压"
 *
 *      hal_adc_init(35, HAL_ADC_RANGE_3_1V);      // GPIO35 = ADC1_CH7
 *
 *      int raw = 0;
 *      hal_adc_read_raw(35, &raw);                // 0 ~ 4095，刻度数
 *
 *      int mv = 0;
 *      if (hal_adc_read_mv(35, &mv))              // 毫伏，走芯片校准
 *      {
 *          printf("%d mV\n", mv);
 *      }
 *
 *   【方式二】连续采样（用 DMA）—— 适合"高速采一段再求平均/分析"
 *
 *      hal_adc_init_continuous(35, HAL_ADC_RANGE_3_1V, 20000);   // 20kHz
 *
 *      while (1)
 *      {
 *          int    buf[128];
 *          size_t got = 0;
 *          if (hal_adc_continuous_read(35, buf, 128, &got, 20) && got > 0)
 *          {
 *              // 处理这批数据
 *          }
 *          vTaskDelay(pdMS_TO_TICKS(10));
 *      }
 *
 *  ==================== 必须知道的几条 ====================
 *
 *   1. 【只用 ADC1，也就是 GPIO32~39】
 *      ESP32 的 ADC2 和 WiFi 共用，WiFi 一开 ADC2 就读不出来了。
 *
 *   2. ADC1 的引脚和通道【不是按顺序排的】：
 *        GPIO36→CH0   GPIO37→CH1   GPIO38→CH2   GPIO39→CH3
 *        GPIO32→CH4   GPIO33→CH5   GPIO34→CH6   GPIO35→CH7
 *      换算在驱动内部做，你只管传 GPIO 号。
 *
 *   3. 量程要选"刚好够用"的：
 *        RANGE_1_1V → 0~1.1V，最精细（1 个刻度 ≈ 0.27mV）
 *        RANGE_1_5V → 0~1.5V
 *        RANGE_2_2V → 0~2.2V
 *        RANGE_3_1V → 0~3.1V，3.3V 系统用这个（1 个刻度 ≈ 0.76mV）
 *      ⚠️ 选太大会浪费分辨率；选太小则超出量程时【静默饱和】——
 *         读数卡在上限不动，不报错，很容易误判成"电压不变化了"。
 *
 *   4. raw 和 mv 是两回事：
 *        raw = 这把尺子上的刻度数（0~4095），不校准，快
 *        mv  = 真实电压（毫伏），用 eFuse 里的校准数据，准
 *      只要判断"亮/暗""往左/往右"，用 raw 就够；要真实数值才用 mv。
 *
 *   5. ESP32 的 ADC 精度一般（典型 ±50mV，且非线性），
 *      适合测电位器、光敏电阻、电池电压，不适合当精密仪表。
 *
 *   6. 【连续模式的两个硬限制】
 *      a) 采样率必须 ≥ 20000 Hz（芯片限制）。所以它【不适合"慢慢读电压"】——
 *         那种场合请用方式一。它的正确用途是"高速采一段，然后求平均"。
 *      b) 一个 ADC 单元只能配一路连续通道（ADC1 的 8 个脚共用同一种模式）。
 *         单次读取模式没有这个限制，同一个单元可以配多个通道。
 *
 *   7. 两种模式按【单元】互斥：ADC1 用了单次读取就不能再用连续采样。
 *      调用会返回 false 并打印原因，不会自动切换——这是刻意的设计。
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

// ADC 模式
typedef enum
{
    HAL_ADC_MODE_NONE = 0,
    HAL_ADC_MODE_ONESHOT,
    HAL_ADC_MODE_CONT,
} hal_adc_mode_t;  // ADC 模式

bool hal_adc_init(int gpio_num, hal_adc_range_t range);
bool hal_adc_read_raw(int gpio_num, int* out_raw);
bool hal_adc_read_mv(int gpio_num, int* out_mv);
// 连续读取 ADC 数据（DMA 模式，高效率）
bool hal_adc_init_continuous(int gpio_num, hal_adc_range_t range, uint32_t sample_hz);
bool hal_adc_continuous_read(int gpio_num, int *raw_buf, size_t max_cnt, size_t *out_cnt, uint32_t timeout_ms);
bool hal_adc_continuous_stop(int gpio_num);
