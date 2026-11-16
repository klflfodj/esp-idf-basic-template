#pragma once

#include <stdbool.h>

// ========== 表情：128x64 OLED 上的卡通眼睛 ========== //
//
// 移植自 eye_all_in_one_esp32，原版 C++/Arduino，这里翻成 C。
// 依赖 drv_oled_draw_bitmap / drv_oled_update，不碰 u8g2。
//
// 典型用法（一个 25fps 的任务）：
//
//    eye_expression_init();
//    eye_expression_set_emotion(EYE_EMOTION_NORMAL);
//
//    while (1)
//    {
//        eye_expression_update();
//        vTaskDelay(pdMS_TO_TICKS(40));
//    }
//
// 三条必须知道的：
//   1. update() 每帧清屏重画，同一屏上不能有第二个渲染者。
//   2. 所有随机行为（眨眼/看/换表情）只在 update() 里推进，
//      不调 update 就全停。
//   3. 这套代码没有加锁：set_emotion / look_at 要和 update 在
//      同一个任务里调；别的任务想改表情，发事件或队列，别直接调。

// 表情编号（顺序和内部预设表一一对应，别随便插）
typedef enum
{
    EYE_EMOTION_NORMAL = 0,     // 正常
    EYE_EMOTION_ANGRY,          // 生气
    EYE_EMOTION_GLEE,           // 坏笑
    EYE_EMOTION_HAPPY,          // 开心
    EYE_EMOTION_SAD,            // 难过
    EYE_EMOTION_WORRIED,        // 担心
    EYE_EMOTION_FOCUSED,        // 专注
    EYE_EMOTION_ANNOYED,        // 烦躁
    EYE_EMOTION_SURPRISED,      // 惊讶
    EYE_EMOTION_SKEPTIC,        // 怀疑
    EYE_EMOTION_FRUSTRATED,     // 沮丧
    EYE_EMOTION_UNIMPRESSED,    // 无语
    EYE_EMOTION_SLEEPY,         // 困
    EYE_EMOTION_SUSPICIOUS,     // 警惕
    EYE_EMOTION_SQUINT,         // 眯眼
    EYE_EMOTION_FURIOUS,        // 暴怒
    EYE_EMOTION_SCARED,         // 害怕
    EYE_EMOTION_AWE,            // 惊叹
    EYE_EMOTION_COUNT,          // 数量，不是有效表情
} eye_emotion_t;

// 初始化：清状态、摆好两只眼的位置、打开随机眨眼和随机看。
// 开机调一次，之后靠 update() 驱动。
void eye_expression_init(void);

// 画一帧并送屏：算过渡、看的方向、眨眼，然后整屏刷新。
// 调用周期建议 40ms（25fps）；越慢越卡，越快 I2C 越吃紧。
void eye_expression_update(void);

// 切换表情：600ms 平滑过渡到新眼型，不会硬跳。
// emotion 超范围（<0 或 >= COUNT）直接忽略。
void eye_expression_set_emotion(eye_emotion_t emotion);

// 眨一次眼：150ms，两只眼同步。
// 随机眨眼开着时不用手动调。
void eye_expression_blink(void);

// 视线转向 (x, y)：x、y 范围 -1 ~ 1，超出会被夹住。
// x 负=看左，正=看右；y 负=看下，正=看上。
// 内部按 12px / 10px 的最大偏移换算，500ms 平滑过去。
void eye_expression_look_at(float x, float y);

// 打开/关闭“随机换表情”：每 5~12 秒随机切一个。
// 默认关闭；打开会立刻重置计时。
void eye_expression_set_random_behavior(bool enable);

// 打开/关闭“随机眨眼”：每 5~12 秒眨一次。默认打开。
void eye_expression_set_random_blink(bool enable);

// 打开/关闭“随机看”：每 5~12 秒随机看一个方向。默认打开。
void eye_expression_set_random_look(bool enable);

// 当前表情编号，用来判断“现在是不是已经在笑了”，避免重复设置。
eye_emotion_t eye_expression_get_emotion(void);
