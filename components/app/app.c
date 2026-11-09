#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"
#include "oled_drv.h"
#include "servo_drv.h"
#include "esp_log.h"


static const char *TAG = "app";


uint32_t o = 0;
static void oled_ui(void)
{
    uint32_t t = o;
    uint8_t s = 0;
    drv_servo_get_angle(&g_servo, &s);
    drv_oled_clear();
    drv_oled_print(1, 1 , "hello OLED");
    drv_oled_printf(3, 1 , "angle = %d °", s);
    drv_oled_printf(4, 1, "o = %d", t);
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
        o++;
        if (o > 10000)
        {
            o = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}


