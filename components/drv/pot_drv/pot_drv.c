#include "pot_drv.h"

#include "adc_hal.h"
#include "esp_log.h"

// 电位器采样次数
#define POT_SAMPLE      16
// 电位器范围最小值
#define POT_SPAN_MIN    500
// 电位器范围最大值
#define POT_RAW_MAX     4095


static const char *TAG = "pot_drv";


// 电位器GPIO引脚号, -1 表示未初始化
static int      s_gpio    = -1;
// 电位器范围最小值
static uint16_t s_min_raw = 0;
// 电位器范围最大值
static uint16_t s_max_raw = 0;
// 电位器是否初始化
static bool     s_inited  = false;
// 电位器是否就绪
static bool     s_ready   = false;

// 初始化电位器
bool pot_init(const pot_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        ESP_LOGE(TAG, "cfg is NULL");
        return false;
    }

    // 检查是否已初始化
    if (s_inited && s_gpio == cfg -> gpio_num)
    {
        return true;
    }

    // 初始化ADC
    if (!hal_adc_init(cfg -> gpio_num, HAL_ADC_RANGE_3_1V))
    {
        ESP_LOGE(TAG, "adc init failed on gpio %d", cfg->gpio_num);
        return false;
    }

    // 检查是否引脚号是否改变
    if (s_inited && s_gpio != cfg -> gpio_num)
    {
        ESP_LOGW(TAG, "pin changed %d -> %d, calibration cleared",
                 s_gpio, cfg->gpio_num);
        s_min_raw = 0;
        s_max_raw = 0;
        s_ready   = false;
    }

    s_gpio = cfg -> gpio_num;
    s_inited = true;

    return true;
}

// 检查电位器是否就绪
bool pot_is_ready(void)
{
    return s_ready;
}

// 获取电位器原始值
uint16_t pot_get_raw(void)
{
    if (!s_inited)
    {
        return 0;
    }

    uint32_t sum = 0;   // 采样原始值总和
    uint8_t  cnt = 0;   // 有效样次数

    // 采样电位器
    for (uint8_t i = 0; i < POT_SAMPLE; i++)
    {
        int raw = 0;

        // 读取电位器原始值
        if (hal_adc_read_raw(s_gpio, &raw))
        {
            // 累加原始值
            sum += (uint32_t)raw;
            cnt++;
        }
    }

    if (cnt == 0)
    {
        return 0;
    }

    // 计算平均值
    return (uint16_t)(sum / cnt);
}

// 读取电位器权限值
bool pot_read_permission(uint16_t *out_permille)
{
    if (out_permille == NULL)
    {
        return false;
    }

    if (!s_ready)
    {
        return false;
    }

    uint16_t raw = pot_get_raw();

    // 检查是否超出范围, 如果超出范围, 则返回0或1000
    if (raw <= s_min_raw)
    {
        *out_permille = 0;
        return true;
    }

    if (raw >= s_max_raw)
    {
        *out_permille = 1000;
        return true;
    }

    // 计算范围
    uint32_t span = (uint32_t)(s_max_raw - s_min_raw);

    // 计算权限值
    *out_permille = (uint16_t)(((uint32_t)(raw - s_min_raw) * 1000u) / span);

    return true;
}

// 设置电位器范围
bool pot_set_range(uint16_t min_raw, uint16_t max_raw)
{
    if (!s_inited)
    {
        ESP_LOGE(TAG, "call pot_init() first");
        return false;
    }

    // 检查是否超出范围
    if (max_raw > POT_RAW_MAX)
    {
        ESP_LOGE(TAG, "max %u > %d", (unsigned)max_raw, POT_RAW_MAX);
        return false;
    }

    // 检查是否超出范围
    if (min_raw >= max_raw)
    {
        ESP_LOGE(TAG, "min %u >= max %u", (unsigned)min_raw, (unsigned)max_raw);
        return false;
    }

    if ((uint32_t)(max_raw - min_raw) < POT_SPAN_MIN)
    {
        ESP_LOGW(TAG, "span %u too small (need >= %d), calibrate again",
                 (unsigned)(max_raw - min_raw), POT_SPAN_MIN);
        return false;   // 拒绝，保持原来的范围不变
    }

    s_min_raw = min_raw;
    s_max_raw = max_raw;
    s_ready   = true;

    ESP_LOGI(TAG, "range set: %u ~ %u (span %u)",
             (unsigned)min_raw, (unsigned)max_raw,
             (unsigned)(max_raw - min_raw));

    return true;
}

// 获取电位器范围
bool pot_get_range(uint16_t *min_raw, uint16_t *max_raw)
{
    if (min_raw == NULL || max_raw == NULL)
    {
        return false;
    }

    if (!s_ready)
    {
        return false;
    }

    *min_raw = s_min_raw;
    *max_raw = s_max_raw;

    return true;
}




