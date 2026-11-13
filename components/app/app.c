#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"

#include "nvs_hal.h"
#include "wifi_hal.h"

#include "oled_drv.h"
#include "servo_drv.h"
#include "key_drv.h"
#include "pot_drv.h"

#include "filter_lowpass_svc.h"

#include "esp_system.h"
#include "esp_log.h"

#define APP_POT_FILTER_SHIFT  2 // 低通滤波器位移,约 80ms

#define WIFI_TEST_SSID "ssid"
#define WIFI_TEST_PASS "password"


static const char *TAG = "app";


static svc_filter_lowpass_t s_pot_filter;// 电位器低通滤波器

static volatile uint16_t key_count = 0;//按键计数器

static volatile bool     s_calib   = false;    /* 是否在校准模式 */
static volatile uint16_t pot_value = 0;//电位器值计数器
static volatile uint16_t s_cal_min = 4095;     /* 见过的最小 raw */
static volatile uint16_t s_cal_max = 0;        /* 见过的最大 raw */

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
                    svc_filter_lowpass_reset(&s_pot_filter);    //量程变了，滤波重新起步
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

    if (!svc_filter_lowpass_init(&s_pot_filter, APP_POT_FILTER_SHIFT))
    {
        ESP_LOGE(TAG, "pot filter init failed");
        return false;
    }

    xTaskCreate(TaskServo, "Servo", 2048, NULL, 5, NULL);
    xTaskCreate(TaskOLED, "OLED", 4096, NULL, 1, NULL);
    xTaskCreate(TaskPot, "Pot", 2048, NULL, 10, NULL);
    xTaskCreate(TaskWiFi, "WiFi", 4096, NULL, 3, NULL);

    return true;
}

void TaskWiFi(void *pvParameters)
{
    (void)pvParameters;

    if (!hal_wifi_init())
    {
        ESP_LOGE(TAG, "wifi init failed");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "heap before connect: %u", (unsigned)esp_get_free_heap_size());

    hal_wifi_connect(WIFI_TEST_SSID, WIFI_TEST_PASS);

    hal_wifi_state_t last      = HAL_WIFI_IDLE;
    uint8_t          heartbeat = 0;   //心跳计数，5 秒一次
    uint8_t          fail_wait = 0;   //失败后等几秒再重试
    uint8_t          conn_wait = 0;   //连接中卡了多少秒

    while (1)
    {
        hal_wifi_state_t now = hal_wifi_get_state();

        if (now != last)
        {
            last = now;

            if (now == HAL_WIFI_CONNECTED)
            {
                char   ip[16] = {0};//IP地址
                int8_t rssi   = 0;//信号强度

                hal_wifi_get_ip(ip, sizeof(ip));
                hal_wifi_get_rssi(&rssi);

                ESP_LOGI(TAG, "connected: ip=%s rssi=%d", ip, (int)rssi);
                ESP_LOGI(TAG, "heap after connect: %u", (unsigned)esp_get_free_heap_size());
            }
            else if (now == HAL_WIFI_FAILED)
            {
                ESP_LOGW(TAG, "failed, reason=%d", hal_wifi_get_disconnect_reason());
            }
            else
            {
                ESP_LOGI(TAG, "state -> %d", (int)now);
            }
        }

        //连接中卡太久：上层计时，强制复位重来
        if (now == HAL_WIFI_CONNECTING)
        {
            if (++conn_wait >= 15)
            {
                conn_wait = 0;
                ESP_LOGW(TAG, "connecting 15s, reset");
                hal_wifi_disconnect();
                fail_wait = 9;      //下一轮就重连
            }
        }
        else
        {
            conn_wait = 0;
        }

        //失败或空闲时，每 10 秒重试一次，方便看它能不能自己回来
        if (now == HAL_WIFI_FAILED || now == HAL_WIFI_IDLE)
        {
            if (++fail_wait >= 10)
            {
                fail_wait = 0;
                ESP_LOGI(TAG, "retry connect");
                hal_wifi_connect(WIFI_TEST_SSID, WIFI_TEST_PASS);
            }
        }
        else
        {
            fail_wait = 0;
        }

        //每 5 秒打一次心跳，卡在 CONNECTING 也能看见
        if (++heartbeat >= 5)
        {
            heartbeat = 0;
            ESP_LOGI(TAG, "heartbeat: state=%d reason=%d heap=%u",
                     (int)now,
                     hal_wifi_get_disconnect_reason(),
                     (unsigned)esp_get_free_heap_size());
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
            // 非校准中：读 raw，滤波，换算成千分比
            uint16_t raw = drv_pot_read_raw();
            // 滤波 raw
            uint16_t filtered =  svc_filter_lowpass_update(&s_pot_filter, raw);
            uint16_t permille = 0;

            if (drv_pot_raw_to_permille(filtered, &permille))
            {
                pot_value = permille;
            }
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


