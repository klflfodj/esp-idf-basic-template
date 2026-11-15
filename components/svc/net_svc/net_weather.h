#pragma once

#include <stdbool.h>

// ========== 天气：HTTP 拉取 + JSON 解析 ========== //
//
// 定位：按需调用，不缓存、不建任务。什么时候拉、拉完存哪，
//       由 app 决定。
//
// 用法：
//    svc_weather_t w;
//
//    if (svc_weather_fetch("北京", &w))      // 用表里的城市名
//    {
//        // w.type / w.temperature / w.quality
//    }
//
//    svc_weather_fetch("101010100", &w);     // 或者直接给城市代码
//
// 必须知道的几条：
//   1. 阻塞，最长 5 秒；别在显示任务里调。
//   2. 没联网直接返回 false，不发请求。
//   3. 只有解析成功才写 out，失败不动 out。
//   4. city 只认表里的省级城市名或 9 位代码，别的返回 false。
//   5. 走 HTTP 明文，只适合公开数据。

// 天气结果
typedef struct
{
    char type[16];          // 天气类型，如 "晴"
    char temperature[8];    // 温度，如 "6.2"
    char quality[8];        // 空气质量，如 "轻度"
} svc_weather_t;

bool svc_weather_fetch(const char *city, svc_weather_t *out_weather);
