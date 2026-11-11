#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"

#include "nvs_hal.h"

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

static volatile uint8_t  s_brightness = 0x7F;  //当前亮度
static volatile bool     s_br_mode    = false; //false=旋钮控舵机 true=旋钮控亮度

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
             if (s_br_mode)      //亮度模式下切走，先把亮度存了
            {
                hal_nvs_set_u8("oled_br", s_brightness);
                ESP_LOGI(TAG, "brightness saved: %u",(unsigned)s_brightness);
            }

            //切换模式
            s_br_mode = !s_br_mode;
            ESP_LOGI(TAG, "mode: %s", s_br_mode ? "brightness" : "servo");
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
                    hal_nvs_set_u16("pot_min", s_cal_min);
                    hal_nvs_set_u16("pot_max", s_cal_max);
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

    //亮度模式：同一个旋钮值换算成亮度
    if (s_br_mode)
    {
        s_brightness = (uint8_t)(pot_value * 255 / 1000);
        drv_oled_set_contrast(s_brightness);
    }

    drv_servo_get_angle(&g_servo, &s);
    drv_oled_clear();

    drv_oled_set_font(DRV_OLED_FONT_CN);
    drv_oled_text(15, 15, "舵机和OLED测试");

    drv_oled_set_font(DRV_OLED_FONT_BIG);
    drv_oled_textf(36, 50, "%03d", s);

    drv_oled_set_font(DRV_OLED_FONT_SMALL);
    drv_oled_textf(15, 60, "%03d", k);
    drv_oled_textf(100, 60, "%04d", pot_value);
    drv_oled_text(110, 15, s_br_mode ? "BR" : "SV");   //当前模式

    drv_oled_update();
}

static bool s_pot_range_load(void)
{
    uint16_t mn = 0, mx = 0;

    // 从 NVS 加载电位器范围
    if (!hal_nvs_get_u16("pot_min", &mn) || !hal_nvs_get_u16("pot_max", &mx))
    {
        ESP_LOGI(TAG, "no saved pot range");
        return false;
    }

    // 设置电位器范围
    if (!drv_pot_set_range(mn, mx))
    {
        // NVS 里存了脏数据（比如旧版本写的），drv 自带的校验挡住了
        ESP_LOGW(TAG, "saved range invalid (%u ~ %u), need recalibration",
                 (unsigned)mn, (unsigned)mx);
        return false;
    }

    ESP_LOGI(TAG, "pot range restored: %u ~ %u", (unsigned)mn, (unsigned)mx);

    return true;
}

static void s_brightness_load(void)
{
    uint8_t br = 0;

    if (!hal_nvs_get_u8("oled_br", &br))
    {
        ESP_LOGI(TAG, "no saved brightness");
        return;
    }

    s_brightness = br;
    drv_oled_set_contrast(br);   //开机就应用，不然要等切到亮度模式才生效
    ESP_LOGI(TAG, "brightness restored: %u",(unsigned)br);
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

    s_pot_range_load();   //尝试从NVS恢复校准范围
    s_brightness_load();   //尝试从NVS恢复亮度

    xTaskCreate(TaskServo, "Servo", 2048, NULL, 5, NULL);
    xTaskCreate(TaskOLED, "OLED", 4096, NULL, 1, NULL);
    xTaskCreate(TaskPot, "Pot", 2048, NULL, 10, NULL);

    return true;
}

void TaskServo(void *pvParameters)
{
    while (1)
    {
        if (!s_br_mode)     //亮度模式下不动舵机
        {
            drv_servo_set_angle(&g_servo, pot_value * 180 / 1000);
        }

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


