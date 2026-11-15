#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ========== WiFi 服务：AP 表 + 重连 + 退避 ========== //
//
// 定位：决定"连哪个、失败了怎么办、什么时候再试"。
//       底层动作全交给 wifi_hal，这里不出现 esp_wifi。
//
// 前置：NVS 已初始化；app 建一个 1Hz 任务调 svc_wifi_update()，
//       不调状态机就停在那儿。
//
// 用法：
//    svc_wifi_init();
//    svc_wifi_set_cb(on_wifi_evt, NULL);
//
//    // 1Hz 任务里
//    svc_wifi_update();
//
//    // 按键里
//    svc_wifi_connect_now();
//
// 必须知道的几条：
//   1. 掉线先重试同一个 AP，失败再换下一个；一整轮都失败才退避。
//   2. 退避 1s/10s/30s/1min/10min/30min/1h，连上即清零。
//   3. connect_now() 只是请求：已连接时忽略；否则清退避、从第一个
//      AP 重跑，最多 1 秒生效（下一次 update）。
//   4. 回调只在"连上"和"已连接后掉线"时触发，尝试失败不触发。
//   5. 回调运行在调用 update() 的任务里，别在里面做耗时操作。
//   6. wifi_name 是可选的，为空时用默认名称。（最长 32 字符，只能用字母数字和 -）

// wifi 状态
typedef enum
{
    SVC_WIFI_IDLE = 0,      // 还没开始
    SVC_WIFI_CONNECTING,    // 正在连某个 AP
    SVC_WIFI_CONNECTED,     // 已连上（有 IP）
    SVC_WIFI_BACKOFF,       // 一轮全失败，等退避
} svc_wifi_state_t;

// wifi 事件
typedef enum
{
    SVC_WIFI_EVT_CONNECTED = 0,     // 连上了
    SVC_WIFI_EVT_DISCONNECTED,      // 连着的链路掉了
} svc_wifi_evt_t;

// wifi 回调函数
typedef void (*svc_wifi_cb_t)(svc_wifi_evt_t evt, void *arg);

bool             svc_wifi_init(const char *wifi_name);
void             svc_wifi_update(void);
svc_wifi_state_t svc_wifi_get_state(void);
bool             svc_wifi_is_connected(void);
bool             svc_wifi_get_ip(char *buf, size_t buf_len);
void             svc_wifi_connect_now(void);
bool             svc_wifi_set_cb(svc_wifi_cb_t cb, void *arg);
