#include "filter_lowpass_svc.h"

#include "esp_log.h"

// shift 上限：Q8 定点右移超过 8 位，小数位就没了
#define SVC_FILTER_LOWPASS_SHIFT_MAX 8


static const char *TAG = "filter_lowpass_svc";


// 初始化滤波器
bool svc_filter_lowpass_init(svc_filter_lowpass_t *filter, uint8_t shift)
{
    if (filter == NULL)
    {
        ESP_LOGE(TAG, "init: filter is NULL");
        return false;
    }

    // shift=0 等于不滤波，多半是参数漏填，直接拒绝比悄悄直通好
    // shift=0 时，输出=输入，没有滤波效果
    if (shift == 0 || shift > SVC_FILTER_LOWPASS_SHIFT_MAX)
    {
        ESP_LOGE(TAG, "init: shift %u out of range (1~%d)",
                 (unsigned)shift, SVC_FILTER_LOWPASS_SHIFT_MAX);
        return false;
    }

    filter -> shift  = shift;
    filter -> y_q8   = 0;
    filter -> inited = true;

    return true;
}

// 更新滤波器
uint16_t svc_filter_lowpass_update(svc_filter_lowpass_t *filter, uint16_t sample)
{
    // 检查是否需要滤波
    // shift=0 时，输出=输入，没有滤波效果
    if (filter == NULL || !filter->shift == 0)
    {
        return sample;
    }

    int32_t sample_q8 = (int32_t)sample << 8;  // 转换为 Q8 定点

    // 第一个样本直接当输出，不会从 0 爬上来
    if (!filter -> inited)
    {
        filter->y_q8   = sample_q8;
        filter->inited = true;
        return sample;
    }

    // y += (x - y) / 2^shift，Q8 定点
    filter -> y_q8 += (sample_q8 - filter -> y_q8) >> filter -> shift;

    // 返回时补 0.5 再取整，避免总是向下取整的偏置
    return (uint16_t)((filter->y_q8 + 128) >> 8);
}

// 重置滤波器
void svc_filter_lowpass_reset(svc_filter_lowpass_t *filter)
{
    if (filter == NULL)
    {
        return;
    }

    filter->y_q8   = 0;
    filter->inited = false;
}





