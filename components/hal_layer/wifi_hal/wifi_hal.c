#include "wifi_hal.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

// IP 字符串最长 "255.255.255.255" + '\0'
#define HAL_WIFI_IP_STR_MAX   16


static const char *TAG = "wifi_hal";


// wifi 当前状态
static volatile hal_wifi_state_t s_state = HAL_WIFI_IDLE; 
// 主动断开标志
static volatile bool s_manual_disconnect = false;
// 最后一次断开原因（IDF 的 wifi_err_reason_t 数值）
static volatile int s_disconnect_reason = 0;
// 连接成功后的 IP 字符串
static char s_ip[HAL_WIFI_IP_STR_MAX];
// STA netif 句柄
static esp_netif_t *s_netif = NULL;
// 是否初始化通过
static bool s_inited = false;


static void hal_wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    
    // 如果是断开事件
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_event_sta_disconnected_t *evt = (wifi_event_sta_disconnected_t *)data;// 断开事件结构体

        s_disconnect_reason = evt->reason;// 记录断开原因
        s_ip[0] = '\0';// 清空 IP 字符串

        // 如果是主动断开
        if (s_manual_disconnect)
        {
            // 这次断开是自己发起的，吞掉
            s_manual_disconnect = false;

            // 如果当前状态不是连接中，才切换到空闲状态
            if (s_state != HAL_WIFI_CONNECTING)
            {
                s_state = HAL_WIFI_IDLE;// 切换到空闲状态
            }
        }
        else
        {
            s_state = HAL_WIFI_FAILED;  // 切换到失败状态
            ESP_LOGW(TAG, "disconnected, reason %d", (int)evt->reason);  // 记录断开原因
        }
        return;
    }

    // 如果是 IP成功事件
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;// 连接成功事件结构体

        (void)esp_ip4addr_ntoa(&evt->ip_info.ip, s_ip, sizeof(s_ip));// 转换 IP 字符串
        s_state = HAL_WIFI_CONNECTED;// 切换到已连接状态
        ESP_LOGI(TAG, "connected, ip %s", s_ip);
    }
}

// 初始化 wifi
bool hal_wifi_init(void)
{
    if (s_inited)
    {
        return true;
    }

    // 初始化 netif
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "netif init failed: %s", esp_err_to_name(err));
        return false;
    }

    // 创建默认事件循环
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "create event loop failed: %s", esp_err_to_name(err));
        return false;
    }

    // 如果没有默认 STA netif，创建一个
    if (s_netif == NULL)
    {
        // 创建默认 STA netif
        s_netif = esp_netif_create_default_wifi_sta();
        if (s_netif == NULL)
        {
            ESP_LOGE(TAG, "create sta netif failed");
            return false;
        }
    }

    // 初始化 wifi
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init_cfg) != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi init failed");
        return false;
    }

    // 设置 wifi 存储为 RAM
    // 配置只放内存：SSID 由上层每次传，不替上层存
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK)
    {
        ESP_LOGE(TAG, "set storage failed");
        return false;
    }

    // 注册 wifi 事件处理函数
    // 监听 WIFI_EVENT_STA_CONNECTED 和 IP_EVENT_STA_GOT_IP 事件
    // 其他事件不处理
    // 注册失败时，会自动重试
    if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,hal_wifi_event_handler,NULL,NULL) != ESP_OK)
    {
        ESP_LOGE(TAG, "register wifi event failed");
        return false;
    }

    // 注册 IP 事件处理函数
    // 监听 IP_EVENT_STA_GOT_IP 事件
    // 其他事件不处理
    // 注册失败时，会自动重试
    if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, hal_wifi_event_handler, NULL, NULL) != ESP_OK)
    {
        ESP_LOGE(TAG, "register ip event failed");
        return false;
    }

    // 设置 wifi 模式为 STA
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
    {
        ESP_LOGE(TAG, "set mode failed");
        return false;
    }

    // 启动 wifi
    if (esp_wifi_start() != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi start failed");
        return false;
    }

    s_state  = HAL_WIFI_IDLE;
    s_inited = true;

    ESP_LOGI(TAG, "init done");
    return true;
}

// 连接 wifi
bool hal_wifi_connect(const char *ssid, const char *password)
{
    if (!s_inited)
    {
        ESP_LOGE(TAG, "call hal_wifi_init() first");
        return false;
    }

    if (ssid == NULL || ssid[0] == '\0')
    {
        ESP_LOGE(TAG, "ssid is empty");
        return false;
    }

    wifi_config_t cfg = {0};

    // 检查 SSID 长度
    if (strlen(ssid) >= sizeof(cfg.sta.ssid))
    {
        ESP_LOGE(TAG, "ssid too long (%u)", (unsigned)strlen(ssid));
        return false;
    }
    // 检查密码长度
    if (password != NULL && strlen(password) >= sizeof(cfg.sta.password))
    {
        ESP_LOGE(TAG, "password too long (%u)", (unsigned)strlen(password));
        return false;
    }

    memcpy(cfg.sta.ssid, ssid, strlen(ssid));// 复制 SSID 到配置结构体

    // 如果密码密码为空，设置为 NULL
    if (password != NULL)
    {
        memcpy(cfg.sta.password, password, strlen(password));
    }

    // 设置认证模式为开放
    cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;   // 允许开放网络；要强制 WPA2 改这里
    cfg.sta.pmf_cfg.required   = false;

    // 如果当前已连接或连接中，先断开连接
    if (s_state == HAL_WIFI_CONNECTED || s_state == HAL_WIFI_CONNECTING)
    {
        s_manual_disconnect = true;
        (void)esp_wifi_disconnect();
    }

    // 设置配置
    if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK)
    {
        ESP_LOGE(TAG, "set config failed");
        s_manual_disconnect = false;
        s_state = HAL_WIFI_FAILED;
        return false;
    }

    s_ip[0] = '\0';
    s_state = HAL_WIFI_CONNECTING;

    // 连接 wifi
    // 非阻塞，立刻返回
    // 连接成功后，会上报 WIFI_EVENT_STA_CONNECTED 事件
    // 连接失败后，会上报 WIFI_EVENT_STA_DISCONNECTED 事件
    // 断开后，会上报 WIFI_EVENT_STA_DISCONNECTED 事件
    // 重连后，会上报 WIFI_EVENT_STA_CONNECTED 事件
    if (esp_wifi_connect() != ESP_OK)
    {
        ESP_LOGE(TAG, "connect failed");
        s_manual_disconnect = false;
        s_state = HAL_WIFI_FAILED;
        return false;
    }

    return true;
}

// 断开 wifi
bool hal_wifi_disconnect(void)
{
    if (!s_inited)
    {
        ESP_LOGE(TAG, "call hal_wifi_init() first");
        return false;
    }

    s_manual_disconnect = true;
    s_state = HAL_WIFI_IDLE;
    s_ip[0] = '\0';

    esp_err_t err = esp_wifi_disconnect();

    // 如果当前未连接，直接返回
    if (err == ESP_ERR_WIFI_NOT_CONNECT)
    {
        s_manual_disconnect = false;
        return true;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "disconnect failed: %s", esp_err_to_name(err));
        s_manual_disconnect = false;
        return false;
    }

    return true;
}

hal_wifi_state_t hal_wifi_get_state(void)
{
    return s_state;
}

// 获取 IP 地址
bool hal_wifi_get_ip(char *buf, size_t buf_len)
{
    // 检查参数
    if (buf == NULL || buf_len == 0) // 缓冲区不能为空
    {
        return false;
    }

    // 如果当前未连接，直接返回
    if (s_state != HAL_WIFI_CONNECTED || s_ip[0] == '\0')
    {
        return false;
    }

    // 检查缓冲区大小
    if (strlen(s_ip) + 1 > buf_len)
    {
        ESP_LOGE(TAG, "buffer too small (need %u)", (unsigned)(strlen(s_ip) + 1));
        return false;
    }

    // 复制 IP 到缓冲区
    memcpy(buf, s_ip, strlen(s_ip) + 1);
    return true;
}

// 获取信号强度，只在 CONNECTED 时有效
bool hal_wifi_get_rssi(int8_t *out_rssi)
{
    // 检查参数
    if (out_rssi == NULL || s_state != HAL_WIFI_CONNECTED)  // 输出参数不能为空且当前已连接
    {
        return false;
    }

    // 获取 AP 信息
    wifi_ap_record_t ap = {0};

    // 检查获取 AP 信息是否成功
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
    {
        return false;
    }

    // 复制信号强度到输出参数
    *out_rssi = ap.rssi;
    return true;
}

// 最近一次断开原因，没有就返回 0
int hal_wifi_get_disconnect_reason(void)
{
    return s_disconnect_reason;
}













