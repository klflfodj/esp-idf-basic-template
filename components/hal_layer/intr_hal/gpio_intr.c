#include "gpio_intr.h"

#include <stdio.h>
#include "driver/gpio.h"

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
        printf("hal_gpio_isr_init: %s\n", esp_err_to_name(err));
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
        printf("hal_gpio_isr_register: call hal_gpio_isr_init() first\n");
        return false;
    }

    esp_err_t err = gpio_isr_handler_add(gpio_num, cb, arg);
    if (err != ESP_OK)
    {
        printf("hal_gpio_isr_register: %s\n", esp_err_to_name(err));
        return false;
    }

    return true;
}

