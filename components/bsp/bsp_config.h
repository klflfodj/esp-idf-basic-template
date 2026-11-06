#pragma once

#include <stdbool.h>

// ========== 板级配置：这块板子接了什么、接在哪个脚 ========== //
//
// 换板子只改这个文件。
// 这里只放宏，【不能放变量】（变量放 bsp.c，否则每个包含它的文件都会有副本）。
//
// 引脚限制（ESP32）：
//   6~11   : 接 flash，绝对不能用
//   34~39  : 只能输入，不能输出 PWM、没有内部上拉
//   ADC    : 只能用 ADC1（GPIO32~39），ADC2 和 WiFi 冲突

// ---------- OLED（I2C） ---------- //
#define BSP_OLED_SCL_PIN        22                             // SCL 引脚
#define BSP_OLED_SDA_PIN        21                             // SDA 引脚
#define BSP_OLED_IIC_FREQ       400000                         // I2C 400kHz
#define BSP_OLED_ADDR           0x3C                           // OLED 地址
#define BSP_OLED_TYPE           DRV_OLED_TYPE_SSD1306_128X64   // OLED 类型

// ---------- 电位器（ADC） ---------- //
// 必须接 ADC1：GPIO32~39
#define BSP_POT_ADC_PIN         35                             // ADC 引脚

// ---------- 舵机（PWM 50Hz） ---------- //
// 注意：不能接 34~39（只能输入）
#define BSP_SERVO_PWM_PIN       25                             // PWM 引脚
#define BSP_SERVO_MIN_US        500                            // 最小脉冲宽度，单位微秒
#define BSP_SERVO_MAX_US        2500                           // 最大脉冲宽度，单位微秒
#define BSP_SERVO_HOME_MS       500                            // 首位角度，单位毫秒，用于校准

// ---------- LED ---------- //
#define BSP_LED1_PIN            13                             // LED 引脚
#define BSP_LED2_PIN            12                             // LED 引脚
#define BSP_LED3_PIN            14                             // LED 引脚
#define BSP_LED_ACTIVE_LEVEL    true

// ---------- 按键 ---------- //
#define BSP_KEY_PIN             4                              // 按键 引脚
#define BSP_KEY_ACTIVE_LEVEL    false                          // 按键 按下时电平
#define BSP_KEY_DEBOUNCE_MS     20                             // 防抖时间，单位毫秒
#define BSP_KEY_LONG_MS         1000                           // 长按时间，单位毫秒，用于判断是否长按
#define BSP_KEY_DOUBLE_MS       300                            // 双击时间，单位毫秒，用于判断是否双击
