#pragma once

#include <stdint.h>
#include <stdbool.h>

// ========== KEY EVENT ========== //
typedef enum
{
    DRV_KEY_EVT_NONE    = 0,
    DRV_KEY_EVT_PRESS   = (1 << 0),  // 按下瞬间
    DRV_KEY_EVT_RELEASE = (1 << 1),  // 松开瞬间
    DRV_KEY_EVT_SINGLE  = (1 << 2),  // 单击
    DRV_KEY_EVT_DOUBLE  = (1 << 3),  // 双击
    DRV_KEY_EVT_MULTI   = (1 << 4),  // 多击（3 击及以上）
    DRV_KEY_EVT_LONG    = (1 << 5),  // 长按
} drv_key_event_t;

// ========== KEY CALLBACK ========== //
typedef void (*drv_key_cb_t)(void *arg, drv_key_event_t evt);

// ========== KEY CONFIG ========== //
typedef struct
{
    int          gpio_num;      // 引脚号
    bool         level_active;  // 按下时的电平状态, true=高电平按下，false=低电平按下
    uint32_t     events;        // 要上报哪些事件：DRV_KEY_EVT_xxx 按位或
    uint32_t     debounce_ms;   // 消抖时间，建议 20
    uint32_t     long_ms;       // 长按门限，建议 1000
    uint32_t     double_ms;     // 多击判定窗口，建议 300
    drv_key_cb_t on_event;      // 事件回调
    void         *arg;          // 回调时原样传回
} drv_key_cfg_t;

typedef struct
{
    drv_key_cfg_t cfg;

    bool     last_level;        // 上一次电平状态
    bool     stable_level;      // 当前稳定电平状态
    uint32_t last_change_tick;  // 上一次电平变化时间戳

    bool     pressed;           // 是否按下
    uint32_t press_tick;        // 确认按下时间戳
    uint8_t  click_cnt;         // 本轮点击次数
    uint32_t last_click_tick;   // 最后一次松开时间戳
    bool     long_reported;     // 本次按住是否已经报过长按
    bool     task_started;      // 扫描任务是否已创建
} drv_key_t;

bool drv_key_init(drv_key_t* key, const drv_key_cfg_t* cfg);

bool drv_key_init_single(drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg);
bool drv_key_init_double(drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg);
bool drv_key_init_multi (drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg);
bool drv_key_init_long  (drv_key_t *key, int gpio_num, drv_key_cb_t cb, void *arg);
bool drv_key_init_any   (drv_key_t *key, int gpio_num, uint32_t events,drv_key_cb_t cb, void *arg);
