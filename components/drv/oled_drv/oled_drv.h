#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "iic_hal.h"

/** OLED 驱动（I2C，支持多种控制器和尺寸）
 *
 *  由 Arduino 版移植，但去掉了型号绑定：
 *    型号 / 尺寸 → 运行时查表
 *    总线 / 引脚 / 地址 → 配置结构体
 *
 *  ==================== 怎么用 ====================
 *
 *    drv_oled_cfg_t cfg =
 *    {
 *        .type      = DRV_OLED_TYPE_SSD1306_128X64,
 *        .bus       = HAL_IIC_BUS_0,
 *        .scl_pin   = 22,
 *        .sda_pin   = 21,
 *        .freq_hz   = 400000,
 *        .addr_7bit = 0x3C,
 *        .font      = DRV_OLED_FONT_NORMAL,  // 开机字体
 *    };
 *
 *    if (!drv_oled_init(&cfg)) { ... }        // 失败会打印原因
 *
 *    drv_oled_clear();
 *    drv_oled_print(1, 1, "Hello");
 *    drv_oled_printf(2, 1, "%d mV", mv);
 *    drv_oled_update();                       // ★ 这一句才真正刷屏
 *
 *  ==================== 两种排版方式 ====================
 *
 *    ① 字符网格（按行/列，从 1 开始）
 *         drv_oled_print(2, 1, "TEMP:");
 *       内部: x = (列-1) * 字宽
 *             y = (行-1) * 字高 + Ascent
 *       适合"第几行开头写点什么"，不用算坐标。
 *
 *    ② 像素坐标（像素级控制）
 *         drv_oled_text(37, 52, "TEMP:");       // y = 基线
 *         drv_oled_text_top(37, 40, "TEMP:");   // y = 文字顶部
 *       适合精确排版、居中、图文混排。
 *
 *    ★ y 默认是【基线】，不是顶部：
 *        基线 = 大多数字母坐的那条线，
 *        g / y / p 的尾巴会伸到基线【下面】。
 *        不想算基线就用 drv_oled_text_top()。
 *
 *  ==================== 换字体 ====================
 *
 *    字体是枚举，不是 u8g2 的字体指针，所以使用处【不用】include "u8g2.h"。
 *
 *    能用的四种（括号里是对应的 u8g2 字体）：
 *        DRV_OLED_FONT_SMALL     6x10，纯 ASCII
 *        DRV_OLED_FONT_NORMAL    8x13，纯 ASCII（默认）
 *        DRV_OLED_FONT_BIG       logisoso28，大号，适合显示数值
 *        DRV_OLED_FONT_CN        wqy12，GB2312 中文
 *
 *    两种设置方式：
 *        bsp 配置里:  .font = DRV_OLED_FONT_BIG,
 *        运行时:      drv_oled_set_font(DRV_OLED_FONT_CN);
 *
 *    ★ 枚举和实际字体在 oled_drv.c 的 s_fonts[] 表里一一对应，
 *      头文件里看不到 u8g2 的符号。
 *
 *    ★ 要加一种字体：在 drv_oled_font_t 的 MAX 前加一项，
 *      再到 s_fonts[] 对应位置补一行字体符号。两处必须同步。
 *
 *    ★ 不填 .font 就是 0，也就是 DRV_OLED_FONT_SMALL，不是 NORMAL，
 *      所以 bsp 里要显式写 DRV_OLED_FONT_NORMAL。
 *
 *    ★ 中文字体很占 flash：wqy12 一个就约 198KB（整份固件约 430KB）。
 *
 *  ==================== 画和刷是分开的 ====================
 *
 *    所有 drv_oled_print / drv_oled_text / drv_oled_draw_* 都只改
 *    【内存里的显存】，必须调 drv_oled_update() 才推到屏幕（约 25ms）。
 *    好处：一次画一堆，最后刷一次，省 I2C 时间。
 *
 *  ==================== 必须知道的几条 ====================
 *
 *    1. 字符网格的行列从 1 开始。能放几行几列由【当前字体】决定：
 *         drv_oled_max_line() / drv_oled_max_col() 可以查
 *       超范围会被忽略（不报错）。
 *
 *    2. 像素坐标不检查边界，超出屏幕的部分由 u8g2 自动裁剪。
 *       长字符串跑出屏幕是正常现象，不会报错。
 *
 *    3. 所有数据都通过 hal_iic_write() 发出，不会绕过 iic_hal。
 *
 *    4. 本驱动只支持【一个 OLED】。要同时驱动两块，需要给 u8g2 传
 *       上下文指针（u8x8_SetUserPtr），当前没做。
 *    5. 亮度用 drv_oled_set_contrast(0~255)，不是 0~100。
 *       OLED 没有背光，不能 PWM 调光，只能改面板的驱动电流。
 *
 *       要点：
 *         a. 立即生效，【不需要】调 drv_oled_update()
 *         b. 不是线性：50 可能还很亮，10 已经看不清
 *         c. 太低会拖影/不均匀，一般 60 以上比较稳
 *         d. 不省电：OLED 功耗主要看点亮了多少像素
 *         e. 掉电不保存，要保存自己写 NVS
 *
 *       把旋钮的 0~1000 换成 0~255：
 *         drv_oled_set_contrast((uint8_t)(p * 255 / 1000));
 */

// OLED 型号枚举
typedef enum  
{
    DRV_OLED_TYPE_SSD1306_128X64 = 0,  // 123x64 OLED SSD1306
    DRV_OLED_TYPE_SH1106_128X64,       // 128x64 OLED SH1106
    DRV_OLED_TYPE_SSD1306_128X32,      // 128x32 OLED SSD1306
    DRV_OLED_TYPE_MAX,
} drv_oled_type_t;

// OLED 字体枚举
typedef enum
{
    DRV_OLED_FONT_SMALL = 0,   // 小号 ASCII 6x10
    DRV_OLED_FONT_NORMAL,      // 中号 ASCII 8x13
    DRV_OLED_FONT_BIG,         // 大号字体，适合显示数值
    DRV_OLED_FONT_CN,          // 中文字体
    DRV_OLED_FONT_MAX,
} drv_oled_font_t;

// OLED 配置结构体
typedef struct
{
    drv_oled_type_t    type;      // OLED 型号
    hal_iic_bus_t  bus;       // I2C 总线
    int            scl_pin;   // SCL 引脚
    int            sda_pin;   // SDA 引脚
    uint32_t       freq_hz;   // I2C 频率（Hz）
    uint8_t        addr_7bit; // I2C 地址（7位）
    drv_oled_font_t font;     // 开机字体（枚举，不是 u8g2 字体指针）
} drv_oled_cfg_t;

bool drv_oled_init(const drv_oled_cfg_t *cfg);

void drv_oled_clear(void);
void drv_oled_update(void);
void drv_oled_set_font(drv_oled_font_t font);
void drv_oled_set_contrast(uint8_t value);
uint8_t drv_oled_get_contrast(void);

/* 当前屏的尺寸 / 能放几行几列（用于自己算布局） */
uint8_t drv_oled_width(void);
uint8_t drv_oled_height(void);
uint8_t drv_oled_max_line(void);
uint8_t drv_oled_max_col(void);

void drv_oled_print(uint8_t Line, uint8_t Column, const char *String);
void drv_oled_printf(uint8_t Line, uint8_t Column, const char *fmt, ...);
void drv_oled_print_num(uint8_t Line, uint8_t Column, float Num,
                    uint8_t DecimalPlaces, uint8_t ShowSign);

void drv_oled_text(uint8_t x, uint8_t y, const char *String);
void drv_oled_textf(uint8_t x, uint8_t y, const char *fmt, ...);
void drv_oled_text_num(uint8_t x, uint8_t y, float Num,
                    uint8_t DecimalPlaces, uint8_t ShowSign);

void drv_oled_draw_pixel(uint8_t x, uint8_t y);
void drv_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void drv_oled_draw_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void drv_oled_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void drv_oled_draw_circle(uint8_t x, uint8_t y, uint8_t r);
void drv_oled_draw_disc(uint8_t x, uint8_t y, uint8_t r);
void drv_oled_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);

void drv_oled_mirror(uint8_t H, uint8_t V);

// 贴一整帧页格式显存（128x64 = 1024 字节），之后要调 drv_oled_update()
void drv_oled_blit(const uint8_t *buf);

// 量当前字体下这段文字的像素宽度（需要在 set_font 之后调用）
uint8_t drv_oled_text_width(const char *str);
