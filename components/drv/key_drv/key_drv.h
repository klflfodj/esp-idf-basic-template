#pragma once

#include <stdint.h>
#include <stdbool.h>

// ========== KEY EVENT ========== //
typedef enum
{
    DRV_KEY_EVENT_NONE        = 0,  // 无事件
    DRV_KEY_EVENT_SINGLE      = 1,  // 单击
    DRV_KEY_EVENT_DOUBLE      = 2,  // 双击
    DRV_KEY_EVENT_MULTI       = 3,  // 多击(3击及以上)
    DRV_KEY_EVENT_LONG        = 4,  // 长按
    DRV_KEY_EVENT_MAX,
} drv_key_event_t;

// ========== KEY CONFIG ========== //
typedef struct
{
    uint8_t gpio_num;   // 引脚号
    bool level_active;  // 按下时电平
} drv_key_cfg_t;

// ========== KEY INSTANCE ========== //
typedef struct
{
    drv_key_cfg_t cfg;       // 配置
    uint8_t event_cnt;       // 按下次数计数
    uint8_t same_level_cnt;  // 电平保持计数
    bool is_pressed;         // 按下状态
    bool in_window;          // 在释放窗口期
    bool scanning;           // 正在扫描中
    uint32_t press_time;     // 按下时间
    uint32_t release_time;   // 释放时间
} drv_key_t;

bool drv_key_init(drv_key_t* key, drv_key_cfg_t* cfg);
 
