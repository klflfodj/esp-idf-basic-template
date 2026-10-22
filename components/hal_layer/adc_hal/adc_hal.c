#include "adc_hal.h"

#include <stdio.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_continuous.h"

#define HAL_ADC_UNIT_CNT   SOC_ADC_PERIPH_NUM           //  ADC 单元数量
#define HAL_ADC_CHAN_CNT   SOC_ADC_MAX_CHANNEL_NUM      //  ADC 通道数量

#define HAL_ADC_FRAME_SAMPLES   128                     //  每个 DMA 帧的样本数
#define HAL_ADC_PARSE_MAX       HAL_ADC_FRAME_SAMPLES   //  最大解析样本数

// 单元句柄
static adc_oneshot_unit_handle_t s_unit_handle[HAL_ADC_UNIT_CNT] = {0};// ADC 单元句柄数组
// 校准句柄
static adc_cali_handle_t s_cali_handle[HAL_ADC_UNIT_CNT][HAL_ADC_CHAN_CNT] = {0};// ADC 校准句柄数组
// 连续读取句柄
static adc_continuous_handle_t s_cont_handle[HAL_ADC_UNIT_CNT] = {0};
// 连续读取通道
static adc_channel_t s_cont_chan[HAL_ADC_UNIT_CNT];
// 通道是否已初始化
static bool            s_chan_ready[HAL_ADC_UNIT_CNT][HAL_ADC_CHAN_CNT];
// 通道当前量程
static hal_adc_range_t s_atten[HAL_ADC_UNIT_CNT][HAL_ADC_CHAN_CNT];
// 单元当前模式
static hal_adc_mode_t s_unit_mode[HAL_ADC_UNIT_CNT];

// HAL 的量程 → IDF 的量程
static adc_atten_t hal_adc_atten_to_idf (hal_adc_range_t atten)
{
    switch (atten)
    {
        case HAL_ADC_RANGE_1_1V: return ADC_ATTEN_DB_0;
        case HAL_ADC_RANGE_1_5V: return ADC_ATTEN_DB_2_5;
        case HAL_ADC_RANGE_2_2V: return ADC_ATTEN_DB_6;
        case HAL_ADC_RANGE_3_1V: return ADC_ATTEN_DB_12;
        default:                 return ADC_ATTEN_DB_12;
    }
}

// 引脚号 → (单元, 通道)
static bool hal_adc_io_resolve(int gpio_num, adc_unit_t* out_unit, adc_channel_t* out_chan)
{
    if (adc_oneshot_io_to_channel(gpio_num, out_unit, out_chan) != ESP_OK)
    {
        printf("adc_hal: gpio %d is not an ADC pin\n", gpio_num);
        return false;
    }
    return true;
}

// 创建单元句柄：第一次用到某个单元时才申请
static bool hal_adc_unit_ensure(adc_unit_t unit)
{
    if (s_unit_handle[unit] != NULL)
    {
        return true;
    }

    adc_oneshot_unit_init_cfg_t init_cfg =
    {
        .unit_id  = unit,  
        .clk_src  = ADC_RTC_CLK_SRC_DEFAULT,  
        .ulp_mode = ADC_ULP_MODE_DISABLE, 
    };

    if (adc_oneshot_new_unit(&init_cfg, &s_unit_handle[unit]) != ESP_OK)
    {
        printf("adc_hal: create adc unit failed\n");
        return false;
    }
    return true;
}

// 准备或复用校准句柄
static bool hal_adc_cali_ensure(adc_unit_t unit, adc_channel_t chan, adc_atten_t atten, hal_adc_range_t hal_atten)
{
    /* 配置没变就复用，避免重复申请资源 */
    if (s_chan_ready[unit][chan] && s_atten[unit][chan] == hal_atten)
    {
        return true;
    }

    /* 量程变了 → 旧句柄作废，必须销毁重建 */
    if (s_cali_handle[unit][chan] != NULL)
    {
        (void)adc_cali_delete_scheme_line_fitting(s_cali_handle[unit][chan]);  
        s_cali_handle[unit][chan] = NULL;
    }

    adc_cali_line_fitting_config_t cali_cfg =
    {
        .unit_id = unit,
        .atten   = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle[unit][chan]) != ESP_OK)
    {
        printf("adc_hal: create calibration failed\n");
        return false;
    }

    s_atten[unit][chan] = hal_atten;
    s_chan_ready[unit][chan] = true;
    return true;
}

// 初始化 ADC 通道
bool hal_adc_init(int gpio_num, hal_adc_range_t range)
{
    if (range >= HAL_ADC_RANGE_MAX)
    {
        printf("adc_hal: invalid range %d\n", range);
        return false;
    }

    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;

    // 检查引脚是否为 ADC 脚
    if (!hal_adc_io_resolve(gpio_num, &unit, &chan))
    {
        return false;
    }

    // 检查单元是否已初始化
    if (s_unit_mode[unit] == HAL_ADC_MODE_CONT)
    {
        printf("adc_hal: adc%d is in continuous mode\n", unit + 1);
        return false;
    }

    // 检查通道是否已初始化
    if (!hal_adc_unit_ensure(unit))
    {
        return false;
    }

    // 转换量程为 IDF 的量程
    adc_atten_t idf_atten = hal_adc_atten_to_idf(range);

    // 配置通道参数
    adc_oneshot_chan_cfg_t chan_cfg =
    {
        .atten = idf_atten,
        .bitwidth = ADC_BITWIDTH_12,
    };

    // 配置通道
     if (adc_oneshot_config_channel(s_unit_handle[unit], chan, &chan_cfg) != ESP_OK)
    {
        printf("adc_hal: config channel failed\n");
        return false;
    }

    // 校准通道（可能失败，失败就直接返回，不记账）
    if (!hal_adc_cali_ensure(unit, chan, idf_atten, range))
    {
        return false;
    }

    // 全部成功之后才记账
    s_unit_mode[unit] = HAL_ADC_MODE_ONESHOT;
    return true;
}

// 读取 ADC 原始值
bool hal_adc_read_raw(int gpio_num, int* out_raw)
{
    if (out_raw == NULL)
    {
        printf("adc_hal: out_raw is NULL\n");
        return false;
    }

    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;
    
    // 检查引脚是否为 ADC 脚
    if (!hal_adc_io_resolve(gpio_num, &unit, &chan))
    {
        return false;
    }

    // 检查通道是否已初始化
    if (!s_chan_ready[unit][chan])
    {
        printf("adc_hal: gpio %d not inited, call hal_adc_init() first\n", gpio_num);
        return false;
    }

    // 读取原始值
    return (adc_oneshot_read(s_unit_handle[unit], chan, out_raw) == ESP_OK);
}

// 读取 mV 值
bool hal_adc_read_mv(int gpio_num, int* out_mv)
{
    if (out_mv == NULL)
    {
        printf("adc_hal: out_mv is NULL\n");
        return false;
    }

    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0; 

    if (!hal_adc_io_resolve(gpio_num, &unit, &chan))
    {
        return false;
    }

    if (!s_chan_ready[unit][chan] || s_cali_handle[unit][chan] == NULL)
    {
        printf("adc_hal: gpio %d not inited, call hal_adc_init() first\n", gpio_num);
        return false;
    } 

     /* IDF 提供的"一步到位"接口：读原始值 + 校准，直接给 mV */
    return (adc_oneshot_get_calibrated_result(s_unit_handle[unit],s_cali_handle[unit][chan],chan, out_mv) == ESP_OK);
}

// 启动连续读取
bool hal_adc_init_continuous(int gpio_num, hal_adc_range_t range, uint32_t sample_hz)
{
    // 检查范围是否有效
    if (range >= HAL_ADC_RANGE_MAX)
    {
        printf("adc_hal: invalid range %d\n", (int)range);
        return false;
    }

    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;

    // 检查引脚是否为 ADC 脚
    if (!hal_adc_io_resolve(gpio_num, &unit, &chan))
    {
        return false;
    }

    // 检查采样频率是否有效
    if (sample_hz < SOC_ADC_SAMPLE_FREQ_THRES_LOW || sample_hz > SOC_ADC_SAMPLE_FREQ_THRES_HIGH)
    {
        printf("adc_hal: sample_hz must be %d ~ %d\n",
               SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
        return false;
    }

    // 检查单元是否冲突
    if (s_unit_mode[unit] == HAL_ADC_MODE_ONESHOT)
    {
        printf("adc_hal: adc%d is used by oneshot mode\n", unit + 1);
        return false;
    }

    // 检查通道是否已初始化
    if (s_unit_mode[unit] == HAL_ADC_MODE_CONT)
    {
        if (s_cont_chan[unit] == chan)
        {
            return true;        // 同一路，幂等
        }
        printf("adc_hal: adc%d continuous already on channel %d\n",
               unit + 1, (int)s_cont_chan[unit]);
        return false;
    }

    // 配置连续读取句柄
    adc_continuous_handle_cfg_t hal_cfg =
    {
        .max_store_buf_size = 4096,
        .conv_frame_size    = SOC_ADC_DIGI_DATA_BYTES_PER_CONV * HAL_ADC_FRAME_SAMPLES,
    };

    // 创建连续读取句柄
    if (adc_continuous_new_handle(&hal_cfg, &s_cont_handle[unit]) != ESP_OK)
    {
        printf("adc_hal: new handle failed\n");
        return false;
    }

    // 配置连续读取模式
    adc_digi_pattern_config_t pattern[1] = 
    {
        {
            .atten     = hal_adc_atten_to_idf(range),
            .channel   = chan,
            .unit      = unit,
            .bit_width = ADC_BITWIDTH_12,
        },
    };

    // 配置连续读取模式
    adc_continuous_config_t cont_cfg =
    {
        .pattern_num    = 1,
        .adc_pattern    = pattern,
        .sample_freq_hz = sample_hz,
        .conv_mode      = (unit == ADC_UNIT_1) ? ADC_CONV_SINGLE_UNIT_1
                                               : ADC_CONV_SINGLE_UNIT_2,
    };

    // 配置连续读取
    if (adc_continuous_config(s_cont_handle[unit], &cont_cfg) != ESP_OK)
    {
        printf("adc_hal: continuous config failed\n");
        (void)adc_continuous_deinit(s_cont_handle[unit]);  // 初始化失败，释放句柄
        s_cont_handle[unit] = NULL;
        return false;
    }

    // 启动连续读取
    if (adc_continuous_start(s_cont_handle[unit]) != ESP_OK)
    {
        printf("adc_hal: continuous start failed\n");
        (void)adc_continuous_deinit(s_cont_handle[unit]);
        s_cont_handle[unit] = NULL;
        return false;
    }

    s_cont_chan[unit] = chan;
    s_unit_mode[unit] = HAL_ADC_MODE_CONT;
    return true;
}

// 读取连续数据
bool hal_adc_continuous_read(int gpio_num, int *raw_buf, size_t max_cnt, size_t *out_cnt, uint32_t timeout_ms)
{
    // 检查参数是否有效
    static adc_continuous_data_t parsed[HAL_ADC_PARSE_MAX];

    if (raw_buf == NULL || out_cnt == NULL || max_cnt == 0)
    {
        return false;
    }

    adc_unit_t  unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;

    // 检查引脚是否为 ADC 脚
    if (!hal_adc_io_resolve(gpio_num, &unit, &chan))
    {
        return false;
    }

    *out_cnt = 0;

    // 检查连续读取是否柄是否有效
    if (s_unit_mode[unit] != HAL_ADC_MODE_CONT)
    {
        printf("adc_hal: continuous not started on adc%d\n", unit + 1);
        return false;
    }

    // 检查最大读取数量是否有效
    uint32_t want = (max_cnt > HAL_ADC_PARSE_MAX) ? HAL_ADC_PARSE_MAX : (uint32_t)max_cnt;

    uint32_t n = 0; // 实际读取数量
    // 读取连续数据取
    esp_err_t err = adc_continuous_read_parse(s_cont_handle[unit], parsed, want, &n, timeout_ms);

    // 处理读取结果
    if (err == ESP_ERR_TIMEOUT)
    {
        return true;
    }
    if (err != ESP_OK)
    {
        return false;
    }

    size_t k = 0;
    // 处理读取结果
    for (uint32_t i = 0; i < n && k < max_cnt; i++)
    {
        // 检查数据是否有效
        if (parsed[i].valid && parsed[i].unit == unit && parsed[i].channel == chan)
        {
            raw_buf[k++] = (int)parsed[i].raw_data;  // 转换为 int 类型
        }
    }

    *out_cnt = k;
    return true;
}

// 停止连续读取
bool hal_adc_continuous_stop(int gpio_num)
{
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t chan = ADC_CHANNEL_0;

    if (!hal_adc_io_resolve(gpio_num, &unit, &chan))
    {
        return false;
    }

    if (s_unit_mode[unit] != HAL_ADC_MODE_CONT)
    {
        return true;
    }

    (void)adc_continuous_stop(s_cont_handle[unit]); // 停止连续读取
    (void)adc_continuous_deinit(s_cont_handle[unit]); // 释放连续读取句柄
    s_cont_handle[unit] = NULL;  
    s_unit_mode[unit] = HAL_ADC_MODE_NONE;
    return true;
}
