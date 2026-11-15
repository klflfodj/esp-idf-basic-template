#pragma once

#include <stdbool.h>
#include <time.h>

// ========== 时间：SNTP 对时 ========== //
//
// 定位：只管"把系统时间弄准"，不建任务、不缓存。
//       WiFi 连上之后调一次 init，之后随时 get。
//
// 用法：
//    svc_time_init();          // 建议放在 wifi 连上的回调里
//
//    struct tm t;
//    if (svc_time_get(&t)) { /* t.tm_hour / t.tm_min */ }
//
// 必须知道的几条：
//   1. 没同步好之前 get 返回 false，显示 "--:--" 即可。
//   2. 时区固定 UTC+8，init 里设。
//   3. init 幂等，重复调没事。
//   4. 同步失败的重试交给 lwIP 自己（指数退避）。

bool svc_time_init(void);
bool svc_time_get(struct tm *out_time);





