#include "app.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp.h"
#include "servo_drv.h"
#include "esp_log.h"


static const char *TAG = "app";


bool app_init(void)
{
    if (!bsp_init())
    {
        ESP_LOGE(TAG, "bsp_init failed");
        return false;
    }

    xTaskCreate(TaskServo, "Servo", 4096, NULL, 5, NULL);

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
