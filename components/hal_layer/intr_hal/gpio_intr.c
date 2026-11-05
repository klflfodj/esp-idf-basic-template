#include "gpio_intr.h"

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"


static const char *TAG = "gpio_intr";


// GPIO中断服务是否已安装
static bool s_isr_service_installed = false;

// 初始化GPIO中断服务
bool hal_gpio_isr_init(void)
{
    if (s_isr_service_installed)
    {
        return true;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "%s", esp_err_to_name(err));
        return false;
    }

    s_isr_service_installed = true;
    return true;
}

// 注册GPIO中断回调
bool hal_gpio_isr_register(int gpio_num, hal_gpio_isr_cb_t cb, void *arg)
{
    if (!GPIO_IS_VALID_GPIO(gpio_num) || cb == NULL)
    {
        return false;
    }

    if (!s_isr_service_installed)
    {
        ESP_LOGE(TAG, "call hal_gpio_isr_init() first");
        return false;
    }

    esp_err_t err = gpio_isr_handler_add(gpio_num, cb, arg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "%s", esp_err_to_name(err));
        return false;
    }

    return true;
}

