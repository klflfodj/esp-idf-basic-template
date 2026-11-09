#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"
#include "oled_drv.h"
#include "servo_drv.h"
#include "u8g2.h"
#include "esp_log.h"


static const char *TAG = "app";


static void oled_ui(void)
{
    uint8_t s = 0;
    drv_servo_get_angle(&g_servo, &s);
    drv_oled_clear();

    drv_oled_set_font(u8g2_font_wqy12_t_gb2312);
    drv_oled_text(2, 10, "舵机和OLED测试");

    drv_oled_set_font(u8g2_font_logisoso28_tf);
    drv_oled_textf(36, 54, "%03d", s);

    drv_oled_update();
}


bool app_init(void)
{
    if (!bsp_init())
    {
        ESP_LOGE(TAG, "bsp_init failed");
        return false;
    }

    xTaskCreate(TaskServo, "Servo", 4096, NULL, 5, NULL);
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


