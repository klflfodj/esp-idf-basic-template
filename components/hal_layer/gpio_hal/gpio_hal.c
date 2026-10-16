#include "gpio_hal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "driver/gpio.h"

static inline bool hal_gpio_check_num(uint8_t gpio_num)
{
    if (gpio_num >= GPIO_NUM_MAX)
    {
        printf("gpio_num is out of range\n");
        return false;
    }
    return true;
}

// 初始化GPIO
bool hal_gpio_init(uint8_t gpio_num, hal_gpio_mode_t mode, hal_gpio_pull_t pull, hal_gpio_intr_t intr)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return false;
    }
    if (mode >= HAL_GPIO_MODE_MAX || mode < 0)
    {
        printf("mode is out of range\n");
        return false;
    }
    if (pull >= HAL_GPIO_PULL_MAX || pull < 0)
    {
        printf("pull is out of range\n");
        return false;
    }
    if (intr >= HAL_GPIO_INTR_MAX || intr < 0)
    {
        printf("intr is out of range\n");
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

// 设置高电平
void hal_gpio_set(uint8_t gpio_num)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return;
    }
    gpio_set_level(gpio_num, 1);
}

// 设置低电平
void hal_gpio_clear(uint8_t gpio_num)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return;
    }
    gpio_set_level(gpio_num, 0);
}

// 切换电平
void hal_gpio_toggle(uint8_t gpio_num)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return;
    }
    gpio_set_level(gpio_num, !gpio_get_level(gpio_num));
}

// 读取电平
bool hal_gpio_read(uint8_t gpio_num)
{
    if (!hal_gpio_check_num(gpio_num))
    {
        return false;
    }
    return gpio_get_level(gpio_num);
}
