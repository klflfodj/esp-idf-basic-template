#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"

#include "oled_drv.h"
#include "servo_drv.h"
#include "key_drv.h"

#include "esp_log.h"


static const char *TAG = "app";

static uint16_t key_count = 0;//按键计数器

static void s_key_cb(void *arg, drv_key_event_t evt)
{
    (void)arg;

    switch (evt)
    {
        case DRV_KEY_EVT_PRESS:

            break;
        case DRV_KEY_EVT_RELEASE:

            break;
        case DRV_KEY_EVT_SINGLE:
        {
            key_count++;
            break;
        }

        case DRV_KEY_EVT_DOUBLE:
        {
            key_count--;
            break;
        }

        case DRV_KEY_EVT_MULTI:
        {
            key_count = 0;
            break;
        }

        case DRV_KEY_EVT_LONG:
        {
   
            break;
        }

        default:
            break;
    }
}

static void oled_ui(void)
{
    uint8_t  s = 0;
    uint16_t k = key_count;
    drv_servo_get_angle(&g_servo, &s);
    drv_oled_clear();

    drv_oled_set_font(DRV_OLED_FONT_CN);
    drv_oled_text(15, 15, "舵机和OLED测试");

    drv_oled_set_font(DRV_OLED_FONT_BIG);
    drv_oled_textf(36, 50, "%03d", s);

    drv_oled_set_font(DRV_OLED_FONT_SMALL);
    drv_oled_textf(15, 60, "%03d", k);

    drv_oled_update();
}


bool app_init(void)
{
    if (!bsp_init())
    {
        ESP_LOGE(TAG, "bsp_init failed");
        return false;
    }
    if (!bsp_key_init(s_key_cb, NULL))
    {
        ESP_LOGE(TAG, "bsp_key_init failed");
        return false;
    }

    xTaskCreate(TaskServo, "Servo", 2048, NULL, 5, NULL);
    xTaskCreate(TaskOLED, "OLED", 4096, NULL, 1, NULL);

    return true;
}

void TaskServo(void *pvParameters)
{
    uint8_t i = 0;
    while (1)
    {
        if (i > 180)
        {
            i = 0;
            drv_servo_move(&g_servo, 0, 1000);
            i++;
        }
        drv_servo_set_angle(&g_servo, i);
        i++;
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void TaskOLED(void *pvParameters)
{
    while(1)
    {
        oled_ui();

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}


