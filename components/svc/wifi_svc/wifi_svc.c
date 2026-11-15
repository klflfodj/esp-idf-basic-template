#include "wifi_svc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi_hal.h"
#include "esp_log.h"


static const char *TAG = "wifi_svc";


// 连接卡多久算失败
#define WIFI_SVC_CONNECT_TIMEOUT_MS   5000

// AP 结构体
typedef struct
{
    const char *ssid;     // AP 名称
    const char *password; // 密码
} wifi_svc_ap_t;

// AP 表：按顺序尝试，填真实值
static const wifi_svc_ap_t s_ap_table[] =
{
    { "ry400", "999999999" },
    { "YOUR_SSID_2", "YOUR_PASSWORD_2" },
};

// AP 数量
#define WIFI_SVC_AP_CNT    ((uint8_t)(sizeof(s_ap_table) / sizeof(s_ap_table[0])))

// 退避表
static const uint32_t s_backoff_ms[] =
{
    1000,       // 1 秒
    10000,      // 10 秒
    30000,      // 30 秒
    60000,      // 1 分钟
    600000,     // 10 分钟
    1800000,    // 30 分钟
    3600000,    // 1 小时
};

// 退避表长度
#define WIFI_SVC_BACKOFF_CNT    ((uint8_t)(sizeof(s_backoff_ms) / sizeof(s_backoff_ms[0])))

static svc_wifi_state_t s_state        = SVC_WIFI_IDLE;  // 当前状态机状态
static uint8_t          s_ap_index     = 0;              // 这一轮试到第几个 AP
static uint8_t          s_cycle        = 0;              // 退避挡位
static uint32_t         s_attempt_tick = 0;              // 本次连接开始时间
static uint32_t         s_next_tick    = 0;              // BACKOFF 到期时间
static bool             s_force        = false;          // 按键请求立即连接
static bool             s_inited       = false;          // 是否已初始化
static svc_wifi_cb_t    s_cb           = NULL;           // 回调函数
static void             *s_cb_arg      = NULL;           // 回调函数参数


// 获取当前时间，单位毫秒
static uint32_t wifi_svc_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);// 获取当前时间，单位毫秒
}

// 连接当前 AP
static void wifi_svc_attempt(void)
{
    // 获取当前 AP
    const wifi_svc_ap_t *ap = &s_ap_table[s_ap_index];

    // 输出连接信息，输出内容为：connect 1/2: YOUR_SSID_1
    ESP_LOGI(TAG, "connect %u/%u: %s",
             (unsigned)(s_ap_index + 1), (unsigned)WIFI_SVC_AP_CNT, ap->ssid);

    s_state        = SVC_WIFI_CONNECTING;  // 连接中
    s_attempt_tick = wifi_svc_now_ms();    // 记录连接开始时间

    if (!hal_wifi_connect(ap->ssid, ap->password))
    {
        ESP_LOGE(TAG, "hal connect failed");
    }
}

// 进入退避状态
static void wifi_svc_backoff(uint32_t now)
{
    uint32_t delay = s_backoff_ms[WIFI_SVC_BACKOFF_CNT - 1];

    // 如果当前退避挡位小于退避表长度，更新退避时间
    // 并更新退避挡位
    if (s_cycle < WIFI_SVC_BACKOFF_CNT)
    {
        delay = s_backoff_ms[s_cycle];// 更新退避时间
        s_cycle++;// 更新退避挡位
    }

    s_next_tick = now + delay;// 更新下一次连接时间
    s_state     = SVC_WIFI_BACKOFF;// 退避中

    // 输出退避信息
    ESP_LOGW(TAG, "all %u APs failed, backoff %u ms (cycle %u)",
             (unsigned)WIFI_SVC_AP_CNT, (unsigned)delay, (unsigned)s_cycle);
}

// 处理 AP 连接失败
static void wifi_svc_ap_failed(uint32_t now)
{
    s_ap_index++;// 切换到下一个 AP

    if (s_ap_index >= WIFI_SVC_AP_CNT)
    {
        s_ap_index = 0;// 循环到第一个 AP
        wifi_svc_backoff(now);// 进入退避状态
    }
    else
    {
        wifi_svc_attempt();// 尝试连接下一个 AP
    }
}

// 处理连接成功
static void wifi_svc_on_connected(void)
{
    char ip[16] = {0};

    s_state = SVC_WIFI_CONNECTED;// 已连接
    s_cycle = 0;// 重置退避挡位

    // 获取 IP 地址
    if (hal_wifi_get_ip(ip, sizeof(ip)))
    {
        ESP_LOGI(TAG, "connected, ip: %s", ip);
    }

    // 调用回调函数
    if (s_cb != NULL)
    {
        s_cb(SVC_WIFI_CONNECTED, s_cb_arg);
    }
}

// 初始化 WiFi 
bool svc_wifi_init(const char *wifi_name)
{
    if (s_inited)
    {
        return true;
    }

    if (!hal_wifi_init(wifi_name))
    {
        ESP_LOGE(TAG, "hal init failed");
        return false;
    }

    s_state        = SVC_WIFI_IDLE;  // 初始化状态机为 IDLE
    s_ap_index     = 0;              // 初始化试到第几个 AP
    s_cycle        = 0;              // 初始化退避挡位
    s_attempt_tick = 0;              // 初始化本次连接开始时间
    s_next_tick    = 0;              // 初始化下一次连接时间
    s_force        = false;          // 初始化按键请求立即连接
    s_inited       = true;           // 标记为已初始化

    return true;
}

// 状态机：根据当前状态和 HAL 状态，更新状态
void svc_wifi_update(void)
{
    if (!s_inited)
    {
        return;
    }

    uint32_t now         = wifi_svc_now_ms();     // 获取当前时间，单位毫秒
    hal_wifi_state_t hal = hal_wifi_get_state();  // 获取 HAL 状态

    if (s_force)
    {
        s_force = false;

        if (s_state != SVC_WIFI_CONNECTED)
        {
            ESP_LOGI(TAG, "force connect");
            s_ap_index = 0;  // 重置试到第几个 AP
            s_cycle    = 0;  // 重置退避挡位
            wifi_svc_attempt();
            return;
        }
    }

    switch (s_state)
    {
        case SVC_WIFI_IDLE:
            s_ap_index = 0;
            wifi_svc_attempt();

            break;

        case SVC_WIFI_CONNECTING:
            if (hal == HAL_WIFI_CONNECTED)
            {
                wifi_svc_on_connected();
            }
            else if (hal == HAL_WIFI_FAILED ||
                     (uint32_t)(now - s_attempt_tick) > WIFI_SVC_CONNECT_TIMEOUT_MS)// 连接超时
            {
                if (hal != HAL_WIFI_FAILED)// 连接失败
                {
                    ESP_LOGW(TAG, "connect timeout");
                    hal_wifi_disconnect();      // 把卡住的连接断掉
                }

                wifi_svc_ap_failed(now);// 处理 AP 连接失败
            }

            break;
        
        case SVC_WIFI_CONNECTED:
            // 如果 HAL 状态不是已连接ED，说明连接丢失
            if (hal != HAL_WIFI_CONNECTED)
            {
                ESP_LOGW(TAG, "link lost, retry same AP");

                if (s_cb != NULL)
                {
                    s_cb(SVC_WIFI_EVT_DISCONNECTED, s_cb_arg);
                }

                // 索引不动，先重试同一个 AP
                wifi_svc_attempt();
            }

            break;

        case SVC_WIFI_BACKOFF:
            // 如果当前时间到了退避时间，重试连接
            if ((int32_t)(now - s_next_tick) >= 0)
            {
                s_ap_index = 0;
                wifi_svc_attempt();
            }

            break;

        default:
            break;
    }
}

// 获取当前状态
svc_wifi_state_t svc_wifi_get_state(void)
{
    return s_state;
}

// 是否已连接
bool svc_wifi_is_connected(void)
{
    return (s_state == SVC_WIFI_CONNECTED);
}

// 获取 IP 地址
bool svc_wifi_get_ip(char *buf, size_t buf_len)
{
    return hal_wifi_get_ip(buf, buf_len);
}


// 按键调用：请求立即重连，下一次 update 生效
void svc_wifi_connect_now(void)
{
    s_force = true;
}

// 设置回调函数
bool svc_wifi_set_cb(svc_wifi_cb_t cb, void *arg)
{
    s_cb     = cb;
    s_cb_arg = arg;
    return true;
}



