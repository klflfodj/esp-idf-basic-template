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

#include "filter_lowpass_svc.h"
#include "wifi_svc.h"
#include "net_time.h"
#include "net_weather.h"

#include "eye_expression.h"

#include "esp_system.h"
#include "esp_log.h"

#define APP_WEATHER_CITY    "天津"           // 天气查询城市
#define APP_WEATHER_PERIOD_MS  (30 * 60 * 1000) // 30分钟更新一次天气

#define APP_POT_FILTER_SHIFT  2 // 低通滤波器位移,约 80ms


static const char *TAG = "app";


static const char *s_week_cn[7] = { "日", "一", "二", "三", "四", "五", "六" };

static QueueHandle_t s_emotion_q;      //按键 → 显示任务
static uint8_t       s_emotion_next = 0;  //只在按键任务里改
static volatile bool s_eye_mode = true;   //true=眼睛 false=数据页

static svc_filter_lowpass_t s_pot_filter;// 电位器低通滤波器

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
            //切换表情,循环播放
            s_emotion_next = (uint8_t)((s_emotion_next + 1) % EYE_EMOTION_COUNT);

            //发送表情到显示任务
            eye_emotion_t e = (eye_emotion_t)s_emotion_next;
            xQueueOverwrite(s_emotion_q, &e);

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
            s_eye_mode = !s_eye_mode;
            ESP_LOGI(TAG, "display: %s", s_eye_mode ? "eyes" : "data");
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

static void s_wifi_cb(svc_wifi_evt_t evt, void *arg)
{
    (void)arg;

    if (evt == SVC_WIFI_EVT_CONNECTED)
    {
        char ip[16] = {0};

        if (svc_wifi_get_ip(ip, sizeof(ip)))
        {
            ESP_LOGI(TAG, "wifi connected, ip %s", ip);
        }

        svc_time_init();
    }
    else
    {
        ESP_LOGW(TAG, "wifi lost");
    }
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

static void oled_ui(void)
{
    svc_time_t t;
    uint8_t  s = 0;

    //亮度模式：同一个旋钮值换算成亮度
    if (s_br_mode)
    {
        s_brightness = (uint8_t)(pot_value * 255 / 1000);
        drv_oled_set_contrast(s_brightness);
    }

    drv_servo_get_angle(&g_servo, &s);
    drv_oled_clear();

    drv_oled_set_font(DRV_OLED_FONT_CN);
    if (svc_time_get(&t))
    {
        drv_oled_textf(15, 15, "%02d-%02d 周%s %02d:%02d",
                       t.month, t.day, s_week_cn[t.week], t.hour, t.minute);
    }
    else
    {
        drv_oled_text(15, 15, "--:--");
    }

    drv_oled_set_font(DRV_OLED_FONT_BIG);
    drv_oled_textf(36, 50, "%03d", s);

    drv_oled_set_font(DRV_OLED_FONT_SMALL);
    drv_oled_textf(100, 60, "%04d", pot_value);
    drv_oled_text(110, 15, s_br_mode ? "BR" : "SV");   //当前模式

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

    s_pot_range_load();   //尝试从NVS恢复校准范围
    s_brightness_load();   //尝试从NVS恢复亮度

    if (!svc_filter_lowpass_init(&s_pot_filter, APP_POT_FILTER_SHIFT))
    {
        ESP_LOGE(TAG, "pot filter init failed");
        return false;
    }

    s_emotion_q = xQueueCreate(1, sizeof(eye_emotion_t));//按键 → 显示任务
    if (s_emotion_q == NULL)
    {
        ESP_LOGE(TAG, "emotion queue create failed");
        return false;
    }

    eye_expression_init();

    xTaskCreate(TaskServo, "Servo", 2048, NULL, 5, NULL);
    xTaskCreate(TaskOLED, "OLED", 4096, NULL, 1, NULL);
    xTaskCreate(TaskPot, "Pot", 2048, NULL, 10, NULL);
    xTaskCreate(TaskWiFi, "WiFi", 4096, NULL, 3, NULL);

    return true;
}

void TaskWiFi(void *pvParameters)
{
    if (!svc_wifi_init("KSKBL"))
    {
        ESP_LOGE(TAG, "wifi init failed");
        vTaskDelete(NULL);
    }

    svc_wifi_set_cb(s_wifi_cb, NULL);

    uint32_t last_weather = 0;  /// 上次查询天气的时间戳

    while (1)
    {
        svc_wifi_update();

        // 当前时间戳
        // 连上后先拉一次，之后每 30 分钟一次
        uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);  

        if (svc_wifi_is_connected() &&
            ((last_weather == 0) || ((uint32_t)(now - last_weather) >= APP_WEATHER_PERIOD_MS)))//30分钟更新一次天气
        {
            svc_weather_t w;

            // 查询天气
            if (svc_weather_fetch(APP_WEATHER_CITY, &w))
            {
                last_weather = now;
                ESP_LOGI(TAG, "weather: %s %s %s", w.type, w.temperature, w.quality);
            }
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
    while (1)
    {
        eye_emotion_t e;

        //按键改了表情，在这里应用（保证和 update 同一个任务）
        if (xQueueReceive(s_emotion_q, &e, 0) == pdTRUE)
        {
            eye_expression_set_emotion(e);
            ESP_LOGI(TAG, "emotion -> %d", (int)e);
        }

        if (s_eye_mode)
        {
            eye_expression_update();
            vTaskDelay(pdMS_TO_TICKS(40));   //25fps
        }
        else
        {
            oled_ui();
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }
}

