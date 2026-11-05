#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "key_drv.h"
#include "led_drv.h"


static const char *TAG = "main";


static drv_key_t s_key  = {0};
static drv_led_t s_led1 = {0};
static drv_led_t s_led2 = {0};
static drv_led_t s_led3 = {0};

// 按键事件回调
static void key_cb(void *arg, drv_key_event_t evt)
{
    (void)arg;

    switch (evt)
    {
        case DRV_KEY_EVT_PRESS:
            printf("[KEY] press\n");
            break;

        case DRV_KEY_EVT_RELEASE:
            printf("[KEY] release\n");
            break;

        case DRV_KEY_EVT_SINGLE:
        {
            printf("[KEY] single -> led1(GPIO13)\n");
            drv_led_on(&s_led1);
            break;
        }

        case DRV_KEY_EVT_DOUBLE:
        {
            printf("[KEY] double -> led2(GPIO12)\n");
            drv_led_on(&s_led2);
            break;
        }

        case DRV_KEY_EVT_MULTI:
        {
            printf("[KEY] multi -> led3(GPIO14)\n");
            drv_led_on(&s_led3);
            break;
        }

        case DRV_KEY_EVT_LONG:
        {
            printf("[KEY] long -> toggle all\n");
            drv_led_toggle(&s_led1);
            drv_led_toggle(&s_led2);
            drv_led_toggle(&s_led3);
            break;
        }

        default:
            break;
    }
}

// 自检：依次让三个灯各亮 300ms，跑 2 轮，完全不经过按键
static void led_selftest(void)
{
    for (int round = 0; round < 2; round++)
    {
        printf("------ selftest round %d ------\n", round);

        printf("led1(GPIO13) on\n");
        drv_led_on(&s_led1);
        vTaskDelay(pdMS_TO_TICKS(300));
        drv_led_off(&s_led1);

        printf("led2(GPIO12) on\n");
        drv_led_on(&s_led2);
        vTaskDelay(pdMS_TO_TICKS(300));
        drv_led_off(&s_led2);

        printf("led3(GPIO14) on\n");
        drv_led_on(&s_led3);
        vTaskDelay(pdMS_TO_TICKS(300));
        drv_led_off(&s_led3);
    }

    printf("------ selftest done ------\n");
}

void app_main(void)
{
    printf("\n==== app start ====\n");

    drv_led_cfg_t led_cfg1 = {.gpio_num = 13, .level_active = true};
    drv_led_cfg_t led_cfg2 = {.gpio_num = 12, .level_active = true};
    drv_led_cfg_t led_cfg3 = {.gpio_num = 14, .level_active = true};

    // 三个 init 的返回值都打出来，失败会连原因一起打印
    printf("led1 init: %s\n", drv_led_init(&s_led1, &led_cfg1) ? "ok" : "FAILED");
    printf("led2 init: %s\n", drv_led_init(&s_led2, &led_cfg2) ? "ok" : "FAILED");
    printf("led3 init: %s\n", drv_led_init(&s_led3, &led_cfg3) ? "ok" : "FAILED");

    led_selftest();

    if (!drv_key_init_any(&s_key, 4,
                          DRV_KEY_EVT_PRESS  | DRV_KEY_EVT_RELEASE |
                          DRV_KEY_EVT_SINGLE | DRV_KEY_EVT_DOUBLE  |
                          DRV_KEY_EVT_MULTI  | DRV_KEY_EVT_LONG,
                          key_cb, &s_key))
    {
        printf("key init FAILED\n");
        return;
    }

    printf("==== ready, press the key ====\n");
}
