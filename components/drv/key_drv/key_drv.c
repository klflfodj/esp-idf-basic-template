#include "key_drv.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "gpio_hal.h"

#define DRV_KEY_SAME_LEVEL_THRESHOLD 5     // 连续同电平次数阈值
#define DRV_KEY_WINDOW_MS            300   // 释放窗口时间
#define DRV_KEY_LONG_PRESS_MS        1000  // 长按时间

bool drv_key_init(drv_key_t* key, drv_key_cfg_t* cfg)
{
    if (cfg == NULL)
    {
        printf("drv_key_init: cfg is NULL\n");
        return false;
    }

    key -> cfg = *cfg;
    key -> event_cnt = 0;
    key -> same_level_cnt = 0;
    key -> is_pressed = false;
    key -> in_window = false;
    key -> scanning = false;
    key -> press_time = 0;
    key -> release_time = 0;

    hal_gpio_intr_t intr = cfg -> level_active ? HAL_GPIO_INTR_POSEDGE : HAL_GPIO_INTR_NEGEDGE;
    hal_gpio_pull_t pull = cfg -> level_active ? HAL_GPIO_PULL_DOWN : HAL_GPIO_PULL_UP;

    if (hal_gpio_init(cfg -> gpio_num, HAL_GPIO_MODE_INPUT, pull, intr) != true)
    {
        printf("drv_key_init: hal_gpio_init failed\n");
        return false;
    }
    return true;
}

static void drv_key_scan(drv_key_t* key)
{
    if (!key->scanning)
    {
        return;
    }

    bool level = hal_gpio_read(key->cfg.gpio_num);
    bool pressed = (level == key-> is_pressed);  //电平判断

    if (!pressed)
    {
        key->same_level_cnt++;
    }
    else
    {
        key->same_level_cnt = 0;
    }

    if (key->same_level_cnt >= DRV_KEY_SAME_LEVEL_THRESHOLD)
    {
        key->is_pressed = level;
    }
}
