#pragma once

#include <stdint.h>
#include <stdbool.h>


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
