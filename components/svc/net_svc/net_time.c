#include "net_time.h"

#include <stdlib.h>

#include "esp_sntp.h"
#include "esp_log.h"


static const char *TAG = "net_time";


#define NET_TIME_TZ        "CST-8"             // UTC+8, 时区
#define NET_TIME_SERVER    "ntp.aliyun.com"    // SNTP 服务器

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
bool svc_time_get(struct tm *out_time)
{
    if (out_time == NULL)
    {
        return false;
    }

    // 等待 SNTP 同步完成
    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    {
        return false;
    }

    time_t now = time(NULL);

    return (localtime_r(&now, out_time) != NULL);// 转换为本地时间
}

