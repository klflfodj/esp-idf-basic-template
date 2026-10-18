#pragma once

#include <stdint.h>
#include <stdbool.h>

/** 按键驱动：消抖 + 单击 / 双击 / 多击 / 长按识别
 *
 *  ==================== 怎么用 ====================
 *
 *    static drv_key_t s_key = {0};        // 必须零初始化，且必须是全局/static
 *
 *    static void on_key(void *arg, drv_key_event_t evt)
 *    {
 *        if (evt == DRV_KEY_EVT_SINGLE) { printf("click\n"); }
 *        if (evt == DRV_KEY_EVT_LONG)   { printf("hold\n");  }
 *    }
 *
 *    void app_main(void)
 *    {
 *        drv_key_init_any(&s_key, 0,                             // 引脚号
 *                         DRV_KEY_EVT_SINGLE | DRV_KEY_EVT_LONG,  // 要哪些事件
 *                         on_key, NULL);                          // 回调 + 透传参数
 *        // 到这里就结束了。驱动内部已经创建了 10ms 扫描任务，
 *        // 之后不需要你再调用任何函数。
 *    }
 *
 *  ==================== 事件时序 ====================
 *   （下面假设 long_ms = 1000，double_ms = 300，单位都是毫秒）
 *
 *    单击：按下 ── 松开 ──(等满 300ms，确认没有第二击)──► SINGLE
 *
 *    双击：按下 ── 松开 ─80ms─ 按下 ── 松开 ──(等满 300ms)──► DOUBLE
 *          注意：中间【不会】多报一个 SINGLE
 *
 *    三击及以上：同理继续累加，最后报 MULTI（只报 MULTI，不报具体击数）
 *
 *    长按：按下 ────────(按住满 1000ms)──► LONG
 *          注意：长按之后松手，【不会】再报 SINGLE / DOUBLE
 *
 *  ==================== 必须知道的几条 ====================
 *
 *    1. drv_key_t 必须零初始化（= {0}），并且必须是全局或 static 变量。
 *       驱动会创建任务长期持有它的指针。写成函数里的局部变量，
 *       函数返回后栈被回收，任务就在读野指针 → 随机崩溃，极难查。
 *       （内部有个 task_started 字段，靠初始值 0 判断"任务建过没有"，
 *        所以零初始化不能省。）
 *
 *    2. on_event 回调运行在驱动自己的任务里（优先级 5），【不是中断】。
 *       所以可以安全地 printf、发队列、操作其他驱动。
 *       但不要在里面做延时或死循环，会拖慢按键响应。
 *
 *    3. 所有时间参数单位都是毫秒（ms）。
 *
 *    4. 单击天然有 double_ms 的延迟：必须等窗口结束才能确认"没有第二击"。
 *       如果只开了 SINGLE（没开 DOUBLE/MULTI），就没有等的必要，
 *       松开立即上报，响应最快。
 *
 *    5. 按键中断不是必须的。本驱动靠 10ms 轮询消抖，最坏延迟 10ms，
 *       对按键完全够用。消抖必须靠连续采样，不能靠中断 ——
 *       因为一次抖动会产生好几个边沿。
 */

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
