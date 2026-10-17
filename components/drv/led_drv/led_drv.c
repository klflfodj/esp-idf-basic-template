#include "led_drv.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "gpio_hal.h"

// 初始化LED
bool drv_led_init(drv_led_t* led, drv_led_cfg_t* cfg)
{
    if (led == NULL || cfg == NULL)
    {
        printf("drv_led_init: parameter is NULL\n");
        return false;
    }

    led->cfg = *cfg;
    
    hal_gpio_pull_t pull = cfg -> level_active ? HAL_GPIO_PULL_DOWN : HAL_GPIO_PULL_UP;

    if (hal_gpio_init(cfg -> gpio_num, HAL_GPIO_MODE_OUTPUT, pull, HAL_GPIO_INTR_DISABLE) != true)
    {
        printf("drv_led_init: hal_gpio_init failed\n");
        return false;
    }

    if (hal_gpio_set_level(cfg -> gpio_num, !cfg -> level_active) != true)
    {
        printf("drv_led_init: hal_gpio_set_level failed\n");
        return false;
    }
    led -> state = LED_DRV_OFF;

    return true;
}

// 打开LED
void drv_led_on(drv_led_t* led)
{
    if (led == NULL)
    {
        return;
    }

    (void)hal_gpio_set_level(led -> cfg.gpio_num, led -> cfg.level_active);
    led -> state = LED_DRV_ON;
}

// 关闭LED
void drv_led_off(drv_led_t* led)
{
    if (led == NULL)
    {
        return;
    }

    (void)hal_gpio_set_level(led -> cfg.gpio_num, !led -> cfg.level_active);
    led -> state = LED_DRV_OFF;
}

// 切换LED状态
void drv_led_toggle(drv_led_t* led)
{
    if (led == NULL)
    {
        return;
    }
    
    if (led -> state == LED_DRV_OFF)
    {
        drv_led_on(led);
    }
    else
    {
        drv_led_off(led);
    }
}
