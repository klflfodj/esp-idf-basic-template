#include "net_weather.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"

#include "wifi_svc.h"


static const char *TAG = "net_weather";


#define NET_WEATHER_URL_PREFIX    "http://t.weather.itboy.net/api/weather/city/"// 天气接口前缀
#define NET_WEATHER_URL_MAX       96        // 最大 URL 长度
#define NET_WEATHER_TIMEOUT_MS    5000      // 超时时间，5 秒
#define NET_WEATHER_BUF_MAX       8192      // 实测响应约 4KB

// 城市名 → 城市代码，只列省级；别的城市直接传代码
typedef struct
{
    const char *name;
    const char *code;
} net_weather_city_t;

// 城市名 → 城市代码，只列省级；别的城市直接传代码
static const net_weather_city_t s_city_table[] =
{
    { "北京",     "101010100" },
    { "上海",     "101020100" },
    { "天津",     "101030100" },
    { "重庆",     "101040100" },
    { "哈尔滨",   "101050101" },
    { "长春",     "101060101" },
    { "沈阳",     "101070101" },
    { "呼和浩特", "101080101" },
    { "石家庄",   "101090101" },
    { "太原",     "101100101" },
    { "西安",     "101110101" },
    { "济南",     "101120101" },
    { "乌鲁木齐", "101130101" },
    { "拉萨",     "101140101" },
    { "西宁",     "101150101" },
    { "兰州",     "101160101" },
    { "银川",     "101170101" },
    { "郑州",     "101180101" },
    { "南京",     "101190101" },
    { "武汉",     "101200101" },
    { "杭州",     "101210101" },
    { "合肥",     "101220101" },
    { "福州",     "101230101" },
    { "南昌",     "101240101" },
    { "长沙",     "101250101" },
    { "贵阳",     "101260101" },
    { "成都",     "101270101" },
    { "广州",     "101280101" },
    { "昆明",     "101290101" },
    { "南宁",     "101300101" },
    { "海口",     "101310101" },
    { "香港",     "101320101" },
    { "澳门",     "101330101" },
    { "台北",     "101340101" },
};

// 城市数量
#define NET_WEATHER_CITY_CNT   (sizeof(s_city_table) / sizeof(s_city_table[0]))

// 接收缓冲放静态区，别放栈上：任务栈只有 4KB
static char   s_buf[NET_WEATHER_BUF_MAX]; // 接收缓冲区
static size_t s_len      = 0;             // 当前接收长度
static bool   s_overflow = false;         // 是否溢出

// 处理 HTTP 事件
static esp_err_t net_weather_http_event(esp_http_client_event_t *evt)
{
    // 处理数据事件
    if (evt -> event_id == HTTP_EVENT_ON_DATA) // 如果是数据事件
    {
        if ((s_len + (size_t)evt->data_len) >= NET_WEATHER_BUF_MAX) // 如果缓冲区溢出
        {
            s_overflow = true;
            return ESP_FAIL;
        }

        memcpy(&s_buf[s_len], evt->data, (size_t)evt->data_len); // 复制数据到缓冲区
        s_len += (size_t)evt->data_len; // 更新当前接收长度
        s_buf[s_len] = '\0'; // 结束符
    }

    return ESP_OK;
}

// 查找城市代码或名称
static const char *net_weather_find_code(const char *city)
{
    bool digits = (city[0] != '\0');

    // 检查是否为数字
    for (const char *p = city; *p != '\0'; p++) // 遍历城市名
    {
        // 如果不是数字，直接返回
        if ((*p < '0') || (*p > '9'))
        {
            digits = false;
            break;
        }
    }

    // 如果是数字，直接返回
    if (digits)
    {
        return city;
    }

    // 遍历城市表，查找城市名
    for (size_t i = 0; i < NET_WEATHER_CITY_CNT; i++)
    {
        // 如果找到城市名，直接返回
        if (strcmp(s_city_table[i].name, city) == 0)
        {
            return s_city_table[i].code;
        }
    }

    return NULL;
}

static bool net_weather_parse(const char *json, svc_weather_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        return false;
    }

    bool ok = false;

    cJSON *data     = cJSON_GetObjectItem(root, "data");     // 数据项
    cJSON *forecast = cJSON_GetObjectItem(data, "forecast"); // 天气预报项
    cJSON *today    = cJSON_GetArrayItem(forecast, 0);       // 今日天气项

    cJSON *wendu   = cJSON_GetObjectItem(data, "wendu");     // 温度项
    cJSON *quality = cJSON_GetObjectItem(data, "quality");   // 质量项
    cJSON *type    = cJSON_GetObjectItem(today, "type");     // 天气类型项

    // 检查字段是否真实存在
    if (cJSON_IsString(wendu) && cJSON_IsString(quality) && cJSON_IsString(type))
    {
        snprintf(out->temperature, sizeof(out->temperature), "%s", wendu->valuestring);  // 复制字符串
        snprintf(out->quality, sizeof(out->quality), "%s", quality->valuestring);
        snprintf(out->type, sizeof(out->type), "%s", type->valuestring);
        ok = true;
    }

    cJSON_Delete(root);  // 释放内存
    return ok;
}

// 获取天气数据
bool svc_weather_fetch(const char *city, svc_weather_t *out_weather)
{
    if ((city == NULL) || (city[0] == '\0') || (out_weather == NULL))
    {
        return false;
    }

    if (!svc_wifi_is_connected())
    {
        ESP_LOGW(TAG, "not connected");
        return false;
    }

    // 查找城市代码或名称
    const char *code = net_weather_find_code(city);
    if (code == NULL)
    {
        ESP_LOGE(TAG, "unknown city '%s'", city);
        return false;
    }

    // 构建 URL
    // url 是天气 API 的 URL，用于获取天气数据
    char url[NET_WEATHER_URL_MAX];
    int  n = snprintf(url, sizeof(url), "%s%s", NET_WEATHER_URL_PREFIX, code);

    // 检查 URL 是否有效
    if ((n < 0) || (n >= (int)sizeof(url)))
    {
        ESP_LOGE(TAG, "url too long");
        return false;
    }

    s_len      = 0;     // 初始化接收长度
    s_overflow = false; // 初始化溢出标志
    s_buf[0]   = '\0';  // 初始化缓冲区

    // 配置 HTTP 客户端
    esp_http_client_config_t cfg =
    {
        .url           = url,                    // 请求 URL
        .timeout_ms    = NET_WEATHER_TIMEOUT_MS, // 请求超时时间
        .method        = HTTP_METHOD_GET,        // 请求方法
        .event_handler = net_weather_http_event, // 事件处理函数
    };

    // 初始化 HTTP 客户端
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "http init failed");
        return false;
    }

    esp_err_t err    = esp_http_client_perform(client);         // 执行请求
    int       status = esp_http_client_get_status_code(client); // 获取状态码

    bool ok = false;

    // 检查请求是否成功
    if ((err == ESP_OK) && !s_overflow && (status == 200) && (s_len > 0))
    {
        ok = net_weather_parse(s_buf, out_weather);// 解析 JSON 数据到结构体

        if (!ok)
        {
            ESP_LOGE(TAG, "parse failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "http failed: %s, status %d, len %u",
                 esp_err_to_name(err), status, (unsigned)s_len);
    }

    // cleanup 内部会关连接
    esp_http_client_cleanup(client);

    return ok;
}






