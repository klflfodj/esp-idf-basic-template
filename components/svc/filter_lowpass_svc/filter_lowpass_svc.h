#pragma once

#include <stdint.h>
#include <stdbool.h>

// ========== 一阶低通（指数平滑）：给一个数，还一个更平滑的数 ========== //
//
// 定位：纯算法。不碰硬件、不建任务、不加锁，状态由调用者持有。
//
// 用法：
//    static svc_filter_lowpass_t s_filter;      // 状态放调用者这里
//
//    svc_filter_lowpass_init(&s_filter, 2);     // 每次跟 1/4
//
//    uint16_t y = svc_filter_lowpass_update(&s_filter, raw);
//
//    svc_filter_lowpass_reset(&s_filter);       // 量程变了，重新起步
//
// 必须知道的几条：
//   1. shift 是 2 的指数：shift=2 → 每次跟 1/4，shift=3 → 1/8。
//      20ms 采一次时，shift=2 约 80ms 跟上，shift=3 约 160ms。
//   2. 第一个样本直接当输出，不会从 0 爬上来。
//   3. 内部用 Q8 定点算，不是 y += (x-y)/K ——
//      整数除法在差值小于 K 时恒为 0，输出会"粘"在某个值上不动。
//   4. 输入输出都是 0~65535，raw / permille / 毫伏都能滤。
//   5. 一个实例只能一个任务用；多路信号各建一个实例。
//   6. 它只管平滑，不管限幅、不管单位换算。

// 滤波器状态
typedef struct
{
    int32_t  y_q8;     // 上次输出，Q8 定点
    uint8_t  shift;    // 平滑系数，除以 2^shift
    bool     inited;   // 是否收到过第一个样本
} svc_filter_lowpass_t;

bool     svc_filter_lowpass_init(svc_filter_lowpass_t *filter, uint8_t shift);
uint16_t svc_filter_lowpass_update(svc_filter_lowpass_t *filter, uint16_t sample);
void     svc_filter_lowpass_reset(svc_filter_lowpass_t *filter);


