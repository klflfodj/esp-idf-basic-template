#pragma once 

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ========== NVS HAL：掉电不丢的键值配置存储 ========== //
//
// 定位：存"设备参数"（校准值、亮度、模式、账号密码）
//       不存运行数据、不存日志（那是 SPIFFS/FAT 的事）
//
// 命名空间固定 "app"，和 IDF 自己的 "phy" / "nvs.net80211" 隔开
//
// 用法：
//    hal_nvs_init();                       // bsp_init() 里第一个调
//
//    // 存
//    hal_nvs_set_u16("pot_min", 60);
//
//    // 读（没存过返回 false，不会打印错误）
//    uint16_t min;
//    if (hal_nvs_get_u16("pot_min", &min)) { ... }
//
//    // 字符串
//    char ssid[33];
//    if (hal_nvs_get_str("wifi_ssid", ssid, sizeof(ssid))) { ... }
//
// 必须知道的几条：
//   1. key 最长 15 字符，区分大小写（"pot_min" != "Pot_Min"）
//   2. 只在"配置真的变了"时调用，别放进循环 —— flash 有擦写寿命
//   3. 不能在中断里调（要操作 flash）
//   4. 每次操作都是 open → 操作 → commit → close：
//      无状态、线程安全、不会忘 commit（代价是慢一点，但配置是低频）
//   5. cfg_ver 版本不符会清空所有配置（升级时用户要重新校准）
//   6. erase_all 会清掉本 namespace 下全部 key，不可撤销

bool hal_nvs_init(void);

bool hal_nvs_set_u8 (const char *key, uint8_t  v);
bool hal_nvs_set_u16(const char *key, uint16_t v);
bool hal_nvs_set_u32(const char *key, uint32_t v);

bool hal_nvs_get_u8 (const char *key, uint8_t  *out);
bool hal_nvs_get_u16(const char *key, uint16_t *out);
bool hal_nvs_get_u32(const char *key, uint32_t *out);

bool hal_nvs_set_str(const char *key, const char *str);
bool hal_nvs_get_str(const char *key, char *buf, size_t buf_len);

bool hal_nvs_erase_key(const char *key);

// 清空本 namespace 下【全部】key
// ⚠️ 会删掉校准值、亮度、模式、以及以后加的所有配置，不可撤销
bool hal_nvs_erase_all(void);
