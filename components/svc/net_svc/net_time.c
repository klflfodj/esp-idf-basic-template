#include "net_time.h"

#include <stdlib.h>
#include <time.h>

#include "esp_sntp.h"
#include "esp_log.h"


static const char *TAG = "net_time";


#define NET_TIME_TZ             "CST-8"             // UTC+8, 时区
#define NET_TIME_SERVER         "ntp.aliyun.com"    // SNTP 服务器
#define NET_TIME_VALID_EPOCH    1700000000 // 2023-01-01 00:00:00, 1700000000 秒级时间戳, 2023-01-01 00:00:00

static bool s_inited = false;

// 初始化时间服务
bool svc_time_init(void)
{
    if (s_inited)
    {
        return true;
    }

    // 设置时区
    setenv("TZ", NET_TIME_TZ, 1);
    tzset();// 刷新时区信息

    // 初始化 SNTP 客户端
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);// 设置为轮询模式，不自动同步
    esp_sntp_setservername(0, NET_TIME_SERVER);// 设置 SNTP 服务器
    esp_sntp_init();// 初始化 SNTP 客户端

    s_inited = true;

    ESP_LOGI(TAG, "sntp started");
    return true;
}

// 获取当前时间
bool svc_time_get(svc_time_t *out_time)
{
    if (out_time == NULL)
    {
        return false;
    }

    // 获取当前时间戳
    time_t now = time(NULL);

    // 检查时间戳是否有效
    if (now < NET_TIME_VALID_EPOCH)
    {
        return false;
    }

    struct tm t;

    if (localtime_r(&now, &t) == NULL)
    {
        return false;
    }

    out_time->year   = (uint16_t)(t.tm_year + 1900);
    out_time->month  = (uint8_t)(t.tm_mon + 1);
    out_time->day    = (uint8_t)t.tm_mday;
    out_time->hour   = (uint8_t)t.tm_hour;
    out_time->minute = (uint8_t)t.tm_min;
    out_time->second = (uint8_t)t.tm_sec;
    out_time->week   = (uint8_t)t.tm_wday;

    return true;
}

