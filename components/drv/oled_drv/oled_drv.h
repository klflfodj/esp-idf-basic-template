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
 *        .font      = NULL,               // NULL = 默认 8x13
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
 *    换字体要用 u8g2 的字体符号，所以【使用处】必须：
 *        #include "u8g2.h"
 *
 *    然后两种填法：
 *        bsp 配置里:  .font = u8g2_font_8x13_tf,
 *        运行时:      drv_oled_set_font(u8g2_font_logisoso28_tf);
 *
 *    【哪些字体能用】不用背，直接去这个文件里搜：
 *        managed_components/nixy4__u8g2/src/core/u8g2_fonts.c
 *        搜 "u8g2_font_" 就能看到全部字体名。
 *
 *    常见的几类：
 *        u8g2_font_6x10_tf              纯 ASCII，6x10
 *        u8g2_font_8x13_tf              纯 ASCII，8x13（默认）
 *        u8g2_font_logisoso28_tf        大号字体，适合显示数值
 *        带 _t_iso_8859_1 后缀的        含 ° µ ± 等 Latin-1 符号
 *        带 wqy 前缀、_gb2312 后缀的     中文（体积大，慎用）
 *
 *    ★ 显示不出 ° 或中文，不是驱动的问题，是【那个字体里没有这个字形】。
 *      8x13_tf 是纯 ASCII，没有 °；要换成带 iso_8859_1 或 unicode 的变体。
 *
 *    ★ 如果链接报 undefined reference to 'u8g2_font_xxx'，
 *      说明那个字体没被编进工程，换一个试试。
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
    const uint8_t  *font;     // 字体数据指针
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




