#include <stdio.h>

#include "app.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


static const char *TAG = "main";


void app_main(void)
{
    if (!app_init())
    {
        ESP_LOGE(TAG, "app_init failed");
        return;
    }
    ESP_LOGI(TAG, "app_init success");
}
