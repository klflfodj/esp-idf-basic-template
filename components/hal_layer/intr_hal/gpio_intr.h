#pragma once

#include <stdint.h>
#include <stdbool.h>

/** GPIO 中断 HAL：负责"中断来了叫谁"
 *
 *  ==================== 怎么用 ====================
 *
 *    static volatile uint32_t s_cnt = 0;
 *
 *    static void my_isr(void *arg)          // 回调在中断里跑
 *    {
 *        (void)arg;
 *        s_cnt++;                           // 只做这一件事
 *    }
 *
 *    void app_main(void)
 *    {
 *        hal_gpio_isr_init();                                    // ① 装服务（只装一次）
 *        hal_gpio_init(0, HAL_GPIO_MODE_INPUT, HAL_GPIO_PULL_UP,
 *                      HAL_GPIO_INTR_NEGEDGE);                   // ② 配引脚 + 打开中断
 *        hal_gpio_isr_register(0, my_isr, NULL);                 // ③ 绑定回调
 *
 *        while (1) { printf("cnt = %lu\n", (unsigned long)s_cnt);
 *                    vTaskDelay(pdMS_TO_TICKS(500)); }
 *    }
 *
 *  ==================== 三步的顺序不能乱 ====================
 *
 *    ① 装服务 → ② 开中断 → ③ 绑回调
 *
 *    为什么：先有服务、再开中断、最后绑回调，中间就不会出现
 *    "中断已经打开、却没人处理"的空窗期。
 *    反过来做，空窗期内来的边沿会一直挂着，之后立刻收到一次过期中断。
 *
 *  ==================== 回调是在中断里跑的，三条铁律 ====================
 *
 *    1. 绝对不能 printf
 *       串口很慢、要等锁，在中断里干这个轻则丢数据，重则看门狗复位。
 *    2. 必须快（几十微秒内）
 *       不能 vTaskDelay、不能等信号量、不能 malloc。
 *    3. 和任务共享的变量必须加 volatile
 *       否则编译器可能把它优化掉，或者读到旧值。
 *
 *    中断里只做一件事：置标志 / 发信号量。真正的处理交给任务。
 *
 *    千万别用"在回调里 printf"来验证中断 —— 那样你只会看到程序卡死或重启。
 */

// ========== 中断回调 ========== //
/** 中断回调函数类型
 *
 *    arg  注册时自己传进来的东西，中断触发时原样还给你。
 *         一般传自己的对象指针（比如 drv_key_t*）。
 *
 *  运行在中断上下文，必须遵守上面那三条铁律。
 */

// ========== GPIO ISR ========== //
typedef void (*hal_gpio_isr_cb_t)(void *arg);


bool hal_gpio_isr_init(void);
bool hal_gpio_isr_register(int gpio_num, hal_gpio_isr_cb_t cb, void *arg);

