#include "gpio_hal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "driver/gpio.h"

// 引脚电平掩码
static uint64_t s_high_mask = 0; 

// 检查GPIO引脚号是否有效
static inline bool hal_gpio_check_num(int gpio_num)
{
    if (!GPIO_IS_VALID_GPIO(gpio_num))
    {
        printf("gpio_num is out of range\n");
        return false;
    }
    return true;
}

// 初始化GPIO
bool hal_gpio_init(int gpio_num, hal_gpio_mode_t mode, hal_gpio_pull_t pull, hal_gpio_intr_t intr)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return false;
    }
    
    gpio_config_t cfg = {0};
    
    cfg.pin_bit_mask = (1ULL << gpio_num);

    switch (mode)
    {
        case HAL_GPIO_MODE_DISABLE:
            cfg.mode = GPIO_MODE_DISABLE;
            break;
        case HAL_GPIO_MODE_OUTPUT:
            cfg.mode = GPIO_MODE_OUTPUT;
            break;
        case HAL_GPIO_MODE_INPUT:
            cfg.mode = GPIO_MODE_INPUT;
            break;
        case HAL_GPIO_MODE_INPUT_OUTPUT:
            cfg.mode = GPIO_MODE_INPUT_OUTPUT;
            break;
        case HAL_GPIO_MODE_OUTPUT_OD:
            cfg.mode = GPIO_MODE_OUTPUT_OD;
            break;
        case HAL_GPIO_MODE_INPUT_OUTPUT_OD:
            cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;
            break;
        default:
            printf("mode is out of range\n");
            return false;
    }

    switch (pull)
    {
        case HAL_GPIO_PULL_NONE:
            cfg.pull_up_en = GPIO_PULLUP_DISABLE;
            cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case HAL_GPIO_PULL_UP:
            cfg.pull_up_en = GPIO_PULLUP_ENABLE;
            cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case HAL_GPIO_PULL_DOWN:
            cfg.pull_up_en = GPIO_PULLUP_DISABLE;
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        default:
            printf("pull is out of range\n");
            return false;
    }

    switch (intr)
    {
        case HAL_GPIO_INTR_DISABLE:
            cfg.intr_type = GPIO_INTR_DISABLE;
            break;
        case HAL_GPIO_INTR_POSEDGE:
            cfg.intr_type = GPIO_INTR_POSEDGE;
            break;
        case HAL_GPIO_INTR_NEGEDGE:
            cfg.intr_type = GPIO_INTR_NEGEDGE;
            break;
        case HAL_GPIO_INTR_ANYEDGE:
            cfg.intr_type = GPIO_INTR_ANYEDGE;
            break;
        case HAL_GPIO_INTR_LOW_LEVEL:
            cfg.intr_type = GPIO_INTR_LOW_LEVEL;
            break;
        case HAL_GPIO_INTR_HIGH_LEVEL:
            cfg.intr_type = GPIO_INTR_HIGH_LEVEL;
            break;
        default:
            printf("intr is out of range\n");
            return false;
    }

    esp_err_t err = gpio_config(&cfg);
    return (err == ESP_OK);
}

// 自由设置GPIO电平
bool hal_gpio_set_level(int gpio_num, bool state)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return false;
    }

    esp_err_t err = gpio_set_level(gpio_num, state ? 1U : 0U);
    if (err != ESP_OK)
    {
        return false;
    }
    
    if (state)
    {
        s_high_mask |= (1ULL << gpio_num);
    }
    else
    {
        s_high_mask &= ~(1ULL << gpio_num);
    }

    return true;
}

// 设置高电平
void hal_gpio_set(int gpio_num)
{
    (void)hal_gpio_set_level(gpio_num, true);
}

// 设置低电平
void hal_gpio_clear(int gpio_num)
{
    (void)hal_gpio_set_level(gpio_num, false);
}

// 切换电平
void hal_gpio_toggle(int gpio_num)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return;
    }
    
    bool next_level = ((s_high_mask >> gpio_num) & 1U) == 0;
    (void)hal_gpio_set_level(gpio_num, next_level);
}

// 读取电平
bool hal_gpio_read(int gpio_num, bool *out_level)
{
    if (!hal_gpio_check_num(gpio_num) || out_level == NULL)
    {
        return false;          
    }

    *out_level = (gpio_get_level(gpio_num) != 0);
    return true;               
}
