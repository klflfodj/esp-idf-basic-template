#pragma once

#include <stdbool.h>
#include <time.h>

// ========== 时间：SNTP 对时 + 本地时间读取 ========== //
//
// 定位：只管"把系统时间弄准"和"读出来"，不建任务、不缓存。
//
// 用法：
//    svc_time_init();        // 建议 WiFi 连上后调一次
//
//    svc_time_t t;
//
//    if (svc_time_get(&t))
//    {
//        // t.year / t.month / t.day
//        // t.hour / t.minute / t.second / t.week
//    }
//
// 必须知道的几条：
//   1. 没对上时 get 返回 false，显示 "--:--" 即可。
//   2. 时区固定 UTC+8，init 里设。
//   3. init 幂等，重复调没事。
//   4. 判断时间是否可用是看系统时间，不依赖 SNTP 状态
//      （那个状态读一次就会被清掉）。

// 本地时间
typedef struct
{
    uint16_t year;      // 年，2026
    uint8_t  month;     // 月，1~12
    uint8_t  day;       // 日，1~31
    uint8_t  hour;      // 时，0~23
    uint8_t  minute;    // 分，0~59
    uint8_t  second;    // 秒，0~59
    uint8_t  week;      // 星期，0=周日, 1=周一 ... 6=周六
} svc_time_t;

bool svc_time_init(void);
bool svc_time_get(svc_time_t *out_time);





