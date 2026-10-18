#pragma once

#include <stdint.h>
#include <stdbool.h>

/** GPIO HAL：把 ESP-IDF 的 GPIO 接口包成自己的，上层不直接依赖 driver/gpio.h
 *
 *  ==================== 怎么用 ====================
 *
 *    // 配一个输出脚（比如驱动 LED）
 *    hal_gpio_init(4, HAL_GPIO_MODE_OUTPUT, HAL_GPIO_PULL_NONE, HAL_GPIO_INTR_DISABLE);
 *    hal_gpio_set(4);                       // 拉高
 *    hal_gpio_clear(4);                     // 拉低
 *    hal_gpio_toggle(4);                    // 翻转
 *    hal_gpio_set_level(4, true);           // 同上，但能拿到成功/失败
 *
 *    // 配一个输入脚（比如读按键）
 *    hal_gpio_init(0, HAL_GPIO_MODE_INPUT, HAL_GPIO_PULL_UP, HAL_GPIO_INTR_DISABLE);
 *    bool level = false;
 *    if (hal_gpio_read(0, &level)) { ... }   // 返回 false 时不要用 level
 *  ==================== 必须知道的几条 ====================
 *
 *    1. gpio_num 必须是本芯片真实存在的引脚。
 *       ESP32 是 0~39，但 24 / 28 / 29 / 30 / 31 这五个号不存在，传了会被拒绝。
 *    2. 34~39 只能做输入，不能输出。
 *    3. 纯输出模式（OUTPUT / OUTPUT_OD）下输入缓冲是关闭的，
 *       调 gpio_get_level() 读回来【恒为 0】。本驱动内部记了一份"影子电平"，
 *       所以 hal_gpio_toggle() 在任何模式下都能正确工作。
 *    4. 【最重要】这份影子电平要求"所有写操作都走本驱动"。
 *       只要哪里绕过 HAL 直接调了 gpio_set_level()，影子就和实际错开，
 *       之后 toggle 会翻错方向，而且不会自己恢复。
 *    5. 本文件只管"引脚是什么形态、电平怎么读写"；
 *       "中断来了叫谁"在 gpio_intr.h 里。
 */

// ========== GPIO MODE ========== //
typedef enum
{
    HAL_GPIO_MODE_DISABLE         = 0,  // 关闭GPIO
    HAL_GPIO_MODE_OUTPUT          = 1,  // 输出
    HAL_GPIO_MODE_INPUT           = 2,  // 输入
    HAL_GPIO_MODE_INPUT_OUTPUT    = 3,  // 输入输出
    HAL_GPIO_MODE_OUTPUT_OD       = 4,  // 开漏输出
    HAL_GPIO_MODE_INPUT_OUTPUT_OD = 5,  // 输入输出开漏
    HAL_GPIO_MODE_MAX,
} hal_gpio_mode_t;

// ========== GPIO PULL ========== //
typedef enum
{
    HAL_GPIO_PULL_NONE            = 0,  // 无上拉/下拉
    HAL_GPIO_PULL_UP              = 1,  // 上拉
    HAL_GPIO_PULL_DOWN            = 2,  // 下拉
    HAL_GPIO_PULL_MAX,
} hal_gpio_pull_t;

// ========== GPIO INT ========== //
typedef enum
{
    HAL_GPIO_INTR_DISABLE          = 0,  // 禁止中断
    HAL_GPIO_INTR_POSEDGE          = 1,  // 上升沿触发
    HAL_GPIO_INTR_NEGEDGE          = 2,  // 下降沿触发
    HAL_GPIO_INTR_ANYEDGE          = 3,  // 任意沿触发
    HAL_GPIO_INTR_LOW_LEVEL        = 4,  // 低电平触发
    HAL_GPIO_INTR_HIGH_LEVEL       = 5,  // 高电平触发
    HAL_GPIO_INTR_MAX,
} hal_gpio_intr_t;


bool hal_gpio_init(int gpio_num, hal_gpio_mode_t mode, hal_gpio_pull_t pull, hal_gpio_intr_t intr);
void hal_gpio_set(int gpio_num);     
void hal_gpio_clear(int gpio_num);   
bool hal_gpio_set_level(int gpio_num, bool state);
void hal_gpio_toggle(int gpio_num);
bool hal_gpio_read(int gpio_num, bool *out_level);        
