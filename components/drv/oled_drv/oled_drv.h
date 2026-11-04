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
 *    oled_cfg_t cfg =
 *    {
 *        .type      = OLED_TYPE_SSD1306_128X64,
 *        .bus       = HAL_IIC_BUS_0,
 *        .scl_pin   = 22,
 *        .sda_pin   = 21,
 *        .freq_hz   = 400000,
 *        .addr_7bit = 0x3C,
 *        .font      = NULL,               // NULL = 默认 8x13
 *    };
 *
 *    if (!oled_init(&cfg)) { ... }        // 失败会打印原因
 *
 *    oled_clear();
 *    oled_print(1, 1, "Hello");
 *    oled_printf(2, 1, "%d mV", mv);
 *    oled_update();                       // ★ 这一句才真正刷屏
 *
 *  ==================== 画和刷是分开的 ====================
 *
 *    所有 oled_print / oled_draw_* 都只改【内存里的显存】，
 *    必须调 oled_update() 才推到屏幕（约 25ms）。
 *    好处：一次画一堆，最后刷一次，省 I2C 时间。
 *
 *  ==================== 必须知道的几条 ====================
 *
 *    1. 行列从 1 开始。能放几行几列由当前字体决定：
 *         oled_max_line() / oled_max_col() 可以查
 *       超范围会被忽略（不报错）。
 *
 *    2. 所有数据都通过 hal_iic_write() 发出，不会绕过 iic_hal。
 *
 *    3. 本驱动只支持【一个 OLED】。要同时驱动两块，需要给 u8g2 传上下文指针
 *       （u8x8_SetUserPtr），当前没做。
 */

// OLED 型号枚举
typedef enum  
{
    OLED_TYPE_SSD1306_128X64 = 0,  // 123x64 OLED SSD1306
    OLED_TYPE_SH1106_128X64,       // 128x64 OLED SH1106
    OLED_TYPE_SSD1306_128X32,      // 128x32 OLED SSD1306
    OLED_TYPE_MAX,
} oled_type_t;

typedef struct
{
    oled_type_t    type;      // OLED 型号
    hal_iic_bus_t  bus;       // I2C 总线
    int            scl_pin;   // SCL 引脚
    int            sda_pin;   // SDA 引脚
    uint32_t       freq_hz;   // I2C 频率（Hz）
    uint8_t        addr_7bit; // I2C 地址（7位）
    const uint8_t  *font;     // 字体数据指针
} oled_cfg_t;

bool oled_init(const oled_cfg_t *cfg);

void oled_clear(void);
void oled_update(void);
void oled_set_font(const uint8_t *font);

/* 当前屏的尺寸 / 能放几行几列（用于自己算布局） */
uint8_t oled_width(void);
uint8_t oled_height(void);
uint8_t oled_max_line(void);
uint8_t oled_max_col(void);

void oled_print(uint8_t Line, uint8_t Column, const char *String);
void oled_printf(uint8_t Line, uint8_t Column, const char *fmt, ...);
void oled_print_num(uint8_t Line, uint8_t Column, float Num,
                    uint8_t DecimalPlaces, uint8_t ShowSign);

void oled_draw_pixel(uint8_t x, uint8_t y);
void oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void oled_draw_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void oled_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void oled_draw_circle(uint8_t x, uint8_t y, uint8_t r);
void oled_draw_disc(uint8_t x, uint8_t y, uint8_t r);
void oled_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);

void oled_mirror(uint8_t H, uint8_t V);




