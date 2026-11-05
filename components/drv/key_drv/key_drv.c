#include "key_drv.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_hal.h"
#include "esp_log.h"


static const char *TAG = "key_drv";


// 记录修改按键事件
static void drv_key_emit(drv_key_t* key, drv_key_event_t evt)
{
    if ((key -> cfg.events & evt) == 0)
    {
        return;
    }
    if (key -> cfg.on_event != NULL)
    {
        key -> cfg.on_event(key -> cfg.arg, evt);
    }
}

// 报告按键点击事件
static void drv_key_report_click(drv_key_t* key)
{
    uint8_t cnt = key -> click_cnt;

    key -> click_cnt = 0; 

    if (cnt == 1)
    {
        drv_key_emit(key, DRV_KEY_EVT_SINGLE);
    }
    else if (cnt == 2)
    {
        drv_key_emit(key, DRV_KEY_EVT_DOUBLE);
    }
    else if (cnt >= 3)
    {
        drv_key_emit(key, DRV_KEY_EVT_MULTI);
    }

}

// 扫描按键状态
static void drv_key_scan(drv_key_t* key)
{
    if (key == NULL)
    {
        return;
    }

    uint32_t now = xTaskGetTickCount();

    bool level = false;
    if (!hal_gpio_read(key -> cfg.gpio_num, &level))
    {
        return;
    }

    bool changed = false;

    if (level != key -> last_level)
    {
        key -> last_level       = level;
        key -> last_change_tick = now;
    }
    else if (now - key -> last_change_tick >= key -> cfg.debounce_ms && key -> stable_level != level)
    {
        key -> stable_level = level;
        changed = true;
    }

    if (changed)
    {
        bool pressed_now = (key -> stable_level == key -> cfg.level_active); 

        if (pressed_now && !key -> pressed)
        {
            key -> pressed      = true;
            key -> press_tick   = now;
            key -> long_reported = false;
            drv_key_emit(key, DRV_KEY_EVT_PRESS);
        }
        else if (!pressed_now && key -> pressed)
        {
            key -> pressed      = false;
            drv_key_emit(key, DRV_KEY_EVT_RELEASE);

            if (key -> long_reported)
            {
                key -> click_cnt = 0;
            }
            else
            {
                if (key -> click_cnt < 225)
                {
                    key -> click_cnt++;
                }
                key -> last_click_tick = now;

                if ((key -> cfg.events & (DRV_KEY_EVT_DOUBLE | DRV_KEY_EVT_MULTI)) == 0)
                {
                    drv_key_report_click(key);
                }
            }
        }
    }

    if (key -> cfg.events & DRV_KEY_EVT_LONG && key -> pressed)
    {
        if (key -> pressed && !key -> long_reported && (now - key -> press_tick) >= key -> cfg.long_ms)
        {
            key -> long_reported = true;
            drv_key_emit(key, DRV_KEY_EVT_LONG);
        }
    }

    if (!key -> pressed && key -> click_cnt > 0 && (now - key -> last_click_tick) >= key -> cfg.double_ms)
    {
        drv_key_report_click(key);
    }
}

// 扫描按键状态任务
static void drv_key_scan_task(void *arg)
{
    drv_key_t *key = (drv_key_t *)arg;

    while (1)
    {
        drv_key_scan(key);
        vTaskDelay(pdMS_TO_TICKS(10));      /* 10ms 一轮 */
    }
}

// 初始化按键驱动
bool drv_key_init(drv_key_t* key,const drv_key_cfg_t* cfg)
{
    if (key == NULL || cfg == NULL)
    {
        ESP_LOGE(TAG, "parameter is NULL"); 
        return false;
    }

    if (cfg -> on_event == NULL || cfg -> events == DRV_KEY_EVT_NONE)
    {
        ESP_LOGE(TAG, "need events and callback"); 
        return false;
    }

    key -> cfg = *cfg;

    hal_gpio_pull_t pull = cfg -> level_active ? HAL_GPIO_PULL_DOWN : HAL_GPIO_PULL_UP;

    if (!hal_gpio_init(cfg->gpio_num, HAL_GPIO_MODE_INPUT, pull, 0))
    {
        ESP_LOGE(TAG, "hal_gpio_init failed"); 
        return false;
    }

    bool level = false;
    if (!hal_gpio_read(cfg -> gpio_num, &level))
    {
        ESP_LOGE(TAG, "hal_gpio_read failed"); 
        return false;
    }

    key -> last_level       = level;
    key -> stable_level     = level;

    key -> last_change_tick = xTaskGetTickCount() - cfg -> debounce_ms;
    
    key -> pressed          = (level == cfg -> level_active);
    key -> press_tick       = 0;
    key -> click_cnt        = 0;
    key -> last_click_tick  = 0;
    key -> long_reported    = false;

    if (!key ->task_started)
    {
        if (xTaskCreate(drv_key_scan_task, "key_scan", 3072, key, 5, NULL) != pdPASS)
        {
            ESP_LOGE(TAG, "xTaskCreate failed"); 
            return false;
        }
        key ->task_started = true;
    }

    return true;
}

// 初始化任意事件
bool drv_key_init_any(drv_key_t *key, int gpio_num, uint32_t events,drv_key_cb_t cb, void *arg)
{
    drv_key_cfg_t cfg = 
    {
        .gpio_num     = gpio_num,
        .level_active = false,          /* 默认低电平按下（配合内部上拉） */
        .events       = events,         
        .debounce_ms  = 20,
        .long_ms      = 1000,
        .double_ms    = 300,
        .on_event     = cb,
        .arg          = arg,
    };

    return drv_key_init(key, &cfg);
}

// 初始化单击事件
bool drv_key_init_single(drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg)
{
    return drv_key_init_any(key, gpio_num, DRV_KEY_EVT_SINGLE, cb, arg);
}

// 初始化双击事件
bool drv_key_init_double(drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg)
{
    return drv_key_init_any(key, gpio_num, DRV_KEY_EVT_DOUBLE, cb, arg);
}

// 初始化多击事件
bool drv_key_init_multi(drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg)
{
    return drv_key_init_any(key, gpio_num, DRV_KEY_EVT_MULTI, cb, arg);
}

// 初始化长按事件
bool drv_key_init_long(drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg)
{
    return drv_key_init_any(key, gpio_num, DRV_KEY_EVT_LONG, cb, arg);
}

