#include "bsp.h"

#include "bsp_config.h"

#include "oled_drv.h"
#include "pot_drv.h"
#include "servo_drv.h"
#include "led_drv.h"
#include "key_drv.h"

#include "nvs_hal.h"

#include "esp_log.h"


static const char *TAG = "bsp";


// ========== 实例定义 ========== //
// extern 在 bsp.h 里声明，实体在这里
// 全局变量默认全 0，所以不需要 = {0}

drv_servo_t   g_servo;
drv_led_t g_led1;
drv_led_t g_led2;
drv_led_t g_led3;
drv_key_t g_key;

// ========== 器件配置 ========== //
// 用指定初始化器，在定义处就把参数填好
// 没写的字段自动是 0，不会是垃圾值

// ---------- OLED ---------- //
static const drv_oled_cfg_t s_oled_cfg =
{
    .type      = BSP_OLED_TYPE,
    .bus       = HAL_IIC_BUS_0,
    .scl_pin   = BSP_OLED_SCL_PIN,
    .sda_pin   = BSP_OLED_SDA_PIN,
    .freq_hz   = BSP_OLED_IIC_FREQ,
    .addr_7bit = BSP_OLED_ADDR,
    .font      = NULL,              // NULL = 驱动用默认字体
};

// ---------- 电位器 ---------- //
static const drv_pot_cfg_t s_pot_cfg =
{
    .gpio_num = BSP_POT_ADC_PIN,
};

// ---------- 舵机 ---------- //
static const drv_servo_cfg_t s_servo_cfg =
{
    .pin     = BSP_SERVO_PWM_PIN,
    .min_us  = BSP_SERVO_MIN_US,
    .max_us  = BSP_SERVO_MAX_US,
    .home_ms = BSP_SERVO_HOME_MS,
};

// ---------- LED ---------- //
/*static drv_led_cfg_t s_led1_cfg =
{
    .gpio_num     = BSP_LED1_PIN,
    .level_active = BSP_LED_ACTIVE_LEVEL,
};

// ---------- LED ---------- //
static drv_led_cfg_t s_led2_cfg =
{
    .gpio_num     = BSP_LED2_PIN,
    .level_active = BSP_LED_ACTIVE_LEVEL,
};

// ---------- LED ---------- //
static drv_led_cfg_t s_led3_cfg =
{
    .gpio_num     = BSP_LED3_PIN,
    .level_active = BSP_LED_ACTIVE_LEVEL,
};*/

// ========== 初始化 ========== //
bool bsp_init(void)
{
    bool ok = true;

    if (!hal_nvs_init())
    {
        ESP_LOGE(TAG, "nvs init failed");
        ok = false;
    }

    if (!drv_oled_init(&s_oled_cfg))
    {
        ESP_LOGE(TAG, "oled init failed");
        ok = false;
    }

    if (!drv_pot_init(&s_pot_cfg))
    {
        ESP_LOGE(TAG, "pot init failed");
        ok = false;
    }

    if (!drv_servo_init(&g_servo, &s_servo_cfg))
    {
        ESP_LOGE(TAG, "servo init failed");
        ok = false;
    }

    /*if (!drv_led_init(&g_led1, &s_led1_cfg))
    {
        ESP_LOGE(TAG, "led1 init failed");
        ok = false;
    }
    if (!drv_led_init(&g_led2, &s_led2_cfg))
    {
        ESP_LOGE(TAG, "led2 init failed");
        ok = false;
    }
    if (!drv_led_init(&g_led3, &s_led3_cfg))
    {
        ESP_LOGE(TAG, "led3 init failed");
        ok = false;
    }*/

    // 按键不在这里：它需要业务回调，用 bsp_key_init()

    ESP_LOGI(TAG, "bsp init %s", ok ? "ok" : "PARTIAL");
    return ok;
}

// ========== 按键初始化 ========== //
bool bsp_key_init(void *cb, void *arg)
{
    if (cb == NULL)
    {
        ESP_LOGE(TAG, "key init: callback is NULL");
        return false;
    }

    // 引脚和时间参数来自板级配置，回调来自 app
    drv_key_cfg_t cfg =
    {
        .gpio_num     = BSP_KEY_PIN,
        .level_active = BSP_KEY_ACTIVE_LEVEL,
        .events       = DRV_KEY_EVT_SINGLE | DRV_KEY_EVT_DOUBLE |
                        DRV_KEY_EVT_MULTI  | DRV_KEY_EVT_LONG,
        .debounce_ms  = BSP_KEY_DEBOUNCE_MS,
        .long_ms      = BSP_KEY_LONG_MS,
        .double_ms    = BSP_KEY_DOUBLE_MS,
        .on_event     = cb,
        .arg          = arg,
    };

    if (!drv_key_init(&g_key, &cfg))
    {
        ESP_LOGE(TAG, "key init failed");
        return false;
    }

    return true;
}

