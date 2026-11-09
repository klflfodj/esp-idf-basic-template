#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"

#include "oled_drv.h"
#include "servo_drv.h"
#include "key_drv.h"
#include "pot_drv.h"

#include "esp_log.h"


static const char *TAG = "app";

static uint16_t key_count = 0;//按键计数器

static bool     s_calib   = false;    /* 是否在校准模式 */
static uint16_t pot_value = 0;//电位器值计数器
static uint16_t s_cal_min = 4095;     /* 见过的最小 raw */
static uint16_t s_cal_max = 0;        /* 见过的最大 raw */

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
            ESP_LOGI(TAG, "long press");

            if (!s_calib)
            {
                s_calib   = true;
                s_cal_min = 4095;
                s_cal_max = 0;
            }
            else
            {
                s_calib = false;
                if (drv_pot_set_range(s_cal_min, s_cal_max))
                {
                    ESP_LOGI(TAG, "calib ok: %u ~ %u",(unsigned)s_cal_min, (unsigned)s_cal_max);
                }
                else
                {
                    ESP_LOGE(TAG, "calib failed");
                }
            }
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
    drv_oled_textf(100, 60, "%04d", pot_value);

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
    xTaskCreate(TaskPot, "Pot", 2048, NULL, 10, NULL);

    return true;
}

void TaskServo(void *pvParameters)
{
    while (1)
    {
        uint8_t servo_angle = pot_value * 180 / 1000;
        drv_servo_set_angle(&g_servo, servo_angle);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void TaskPot(void *pvParameters)
{
    while(1)
    {
        if (s_calib)
        {
            /* 校准中：只读 raw，更新极值 */
            uint16_t raw = drv_pot_read_raw();

            if (raw < s_cal_min) s_cal_min = raw;
            if (raw > s_cal_max) s_cal_max = raw;
        }
        else 
        {
            drv_pot_read_permille(&pot_value);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
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


