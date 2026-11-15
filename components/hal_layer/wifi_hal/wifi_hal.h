#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ========== WiFi HAL：把 esp_wifi 包成"连一次"的动作 ========== //
//
// 定位：只负责发起连接、汇报事实。重试、换 AP、退避、对时、
//       HTTP 都不在这里。
//
// 前置：调用前 NVS 必须已经初始化（bsp_init 里做了），
//       WiFi 自己要往 NVS 写射频校准数据。
//
// 用法：
//    hal_wifi_init();
//    hal_wifi_connect(ssid, password);     // 非阻塞，立刻返回
//
//    // 上层自己轮询
//    switch (hal_wifi_get_state())
//    {
//        case HAL_WIFI_CONNECTED: ... break;
//        case HAL_WIFI_FAILED:    ... break;
//    }
//
// 必须知道的几条：
//   1. CONNECTED = 已经拿到 IP，不是"关联成功"。
//   2. connect() 不等待结果；超时由调用者自己计时。
//   3. 掉线只上报 FAILED，不会自动重连。
//   4. 单实例，没有 handle。
//   5. 断开原因返回原始错误码，不把 IDF 类型放进头文件。

// wifi 状态
typedef enum
{
    HAL_WIFI_IDLE = 0,      // 没在连
    HAL_WIFI_CONNECTING,    // 连接中（还没拿到 IP）
    HAL_WIFI_CONNECTED,     // 已连接且拿到 IP
    HAL_WIFI_FAILED,        // 失败或掉线
} hal_wifi_state_t;


bool hal_wifi_init(const char *wifi_name);
bool hal_wifi_connect(const char *ssid, const char *password);
bool hal_wifi_disconnect(void);
hal_wifi_state_t hal_wifi_get_state(void);
bool hal_wifi_get_ip(char *buf, size_t buf_len);
bool hal_wifi_get_rssi(int8_t *out_rssi);
int  hal_wifi_get_disconnect_reason(void);








