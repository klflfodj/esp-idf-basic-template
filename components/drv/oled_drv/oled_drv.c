#include "oled_drv.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "esp_rom_sys.h"
#include "u8g2.h"
#include "iic_hal.h"
#include "esp_log.h"

// OLED 初始化函数指针
typedef void (*drv_oled_setup_fn_t)(u8g2_t *u8g2, const u8g2_cb_t *rotation,
                                u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb);

// OLED 型号
typedef struct
{
    drv_oled_setup_fn_t setup;  // OLED 初始化函数指针
    uint8_t         width;  // OLED 宽度
    uint8_t         height; // OLED 高度
} drv_oled_model_t;


static const char *TAG = "oled_drv";


static uint8_t s_contrast = 0x7F;   //当前对比度，0x7F是SSD1306出厂默认值
// OLED 型号数组
static const drv_oled_model_t s_model[DRV_OLED_TYPE_MAX] = 
{
    [DRV_OLED_TYPE_SSD1306_128X64] = { u8g2_Setup_ssd1306_i2c_128x64_noname_f,    128, 64 },
    [DRV_OLED_TYPE_SH1106_128X64]  = { u8g2_Setup_sh1106_i2c_128x64_noname_f,     128, 64 },
    [DRV_OLED_TYPE_SSD1306_128X32] = { u8g2_Setup_ssd1306_i2c_128x32_univision_f, 128, 32 },
};

// OLED 字体数组
static const uint8_t *const  s_fonts[DRV_OLED_FONT_MAX] =
{
    [DRV_OLED_FONT_SMALL]  = u8g2_font_6x10_tf,
    [DRV_OLED_FONT_NORMAL] = u8g2_font_8x13_tf,
    [DRV_OLED_FONT_BIG]    = u8g2_font_logisoso28_tf,
    [DRV_OLED_FONT_CN]     = u8g2_font_wqy12_t_gb2312,
};


static u8g2_t        s_u8g2;      // OLED 实例
static hal_iic_bus_t s_bus;       // I2C 总线

static uint8_t       s_width;     // OLED 宽度
static uint8_t       s_height;    // OLED 高度

static uint8_t       FontWidth;   // 字体宽度
static uint8_t       FontHeight;  // 字体高度
static uint8_t       FontAscent;  // 字体上边距

static bool          s_ready;     // OLED 是否初始化

// 没初始化就静默返回，免得对着未初始化的显存乱发数据
#define OLED_GUARD() do{ if (!s_ready) { return; } } while(0)


#define OLED_BUF_MAX  160

static uint8_t        s_buf[OLED_BUF_MAX]; // OLED 缓冲区
static uint16_t       s_buf_len = 0;       // 缓冲区长度
static uint8_t        s_addr    = 0x3C;    // OLED 地址

// OLED 字节回调函数
static uint8_t drv_oled_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
        case U8X8_MSG_BYTE_INIT:
            return 1;
        
        case U8X8_MSG_BYTE_START_TRANSFER:
         // u8g2 内部存 8 位地址(0x78)，hal_iic_write 要 7 位(0x3C)。
            s_addr    = ((uint8_t)(u8x8_GetI2CAddress(u8x8) >> 1));  // 右移 1 位，去掉最高位
            s_buf_len = 0;  // 清空缓冲区
            return 1;

        case U8X8_MSG_BYTE_SEND:
            if ((uint32_t)s_buf_len + arg_int > OLED_BUF_MAX)
            {
                ESP_LOGE(TAG, "buf overflow (%u + %u)",
                       (unsigned)s_buf_len, (unsigned)arg_int);// 缓冲区溢出，返回 0
                return 0;
            }
            // 复制数据到缓冲区
            memcpy (&s_buf[s_buf_len], arg_ptr, arg_int);
            // 缓冲区长度增加
            s_buf_len += arg_int;
            return 1;

        case U8X8_MSG_BYTE_END_TRANSFER:
        {
            // 缓冲区为空，直接返回
            if (s_buf_len == 0)
            {
                return 1;
            }

            // 发送数据
            bool ok = hal_iic_write(s_bus, s_addr, s_buf, s_buf_len);

            s_buf_len = 0;  // 清空缓冲区
            return ok ? 1 : 0;
        }

        default:
            return 1;
    }
}

// OLED GPIO 延时回调函数
static uint8_t drv_oled_gpio_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
        // 毫秒延时
        case U8X8_MSG_DELAY_MILLI:
            esp_rom_delay_us((uint32_t)arg_int * 1000u); // 转换为微秒延时
            return 1;

        // 微秒延时
        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us((uint32_t)arg_int * 10u); // 转换为纳秒延时
            return 1;

        // 纳秒延时
        case U8X8_MSG_DELAY_100NANO:
            esp_rom_delay_us(1); // 转换为纳秒延时
            return 1;

        default:
            return 1;
    }
}

// OLED 宽度
uint8_t drv_oled_width(void)       { return s_width; }
// OLED 高度
uint8_t drv_oled_height(void)      { return s_height; }

// OLED 最大行数
uint8_t drv_oled_max_line(void)
{
    return (FontHeight > 0) ? (uint8_t)(s_height / FontHeight) : 0;
}

// OLED 最大列数
uint8_t drv_oled_max_col(void)
{
    return (FontWidth > 0) ? (uint8_t)(s_width / FontWidth) : 0;
}

// OLED 初始化
bool drv_oled_init(const drv_oled_cfg_t *cfg)
{
    // 检查配置是否有效
    if (cfg == NULL)
    {
        ESP_LOGE(TAG, "cfg is NULL"); 
        return false;
    }

    // 检查型号是否有效
    if ((int)cfg->type < 0 || cfg->type >= DRV_OLED_TYPE_MAX)
    {
        ESP_LOGE(TAG, "invalid type %d", (int)cfg->type); 
        return false;
    }

    // 检查地址是否为 7 位
    if (cfg->addr_7bit > 0x7F)
    {
        ESP_LOGE(TAG, "addr 0x%02X is not 7-bit (try >> 1)", cfg->addr_7bit); 
        return false;
    }

    const drv_oled_model_t *model = &s_model[cfg -> type];

    hal_iic_cfg_t iic_cfg = 
    {
        .bus     = cfg->bus,
        .scl_pin = cfg->scl_pin,
        .sda_pin = cfg->sda_pin,
        .freq_hz = cfg->freq_hz,
    };

    if (!hal_iic_init(&iic_cfg))
    {
        ESP_LOGE(TAG, "iic init failed"); 
        return false;
    }

    if (!hal_iic_probe(cfg->bus, cfg->addr_7bit))
    {
        ESP_LOGE(TAG, "no device at 0x%02X (check wiring / addr / pullup)", cfg->addr_7bit); 
        return false;
    }

    s_bus    = cfg->bus;        // 记录总线号
    s_width  = model->width;    // 记录宽度
    s_height = model->height;   // 记录高度

    // 初始化 OLED
    model->setup(&s_u8g2, U8G2_R0, drv_oled_byte_cb, drv_oled_gpio_delay_cb);

    // 设置 OLED 地址
    u8x8_SetI2CAddress(u8g2_GetU8x8(&s_u8g2), (uint8_t)(cfg->addr_7bit << 1));

    // 初始化显示
    u8g2_InitDisplay(&s_u8g2);
    // 关闭显示
    u8g2_SetPowerSave(&s_u8g2, 0);

    drv_oled_set_font((cfg->font != NULL) ? (drv_oled_font_t)cfg->font : DRV_OLED_FONT_NORMAL);

    s_ready = true;  // 标记为已初始化

    drv_oled_clear();
    drv_oled_update();

    return true;
}

// 清空显示缓冲区
void drv_oled_clear(void)
{
    OLED_GUARD();
    u8g2_ClearBuffer(&s_u8g2);  // 清空显示缓冲区
}

// 更新显示
void drv_oled_update(void)
{
    OLED_GUARD();
    u8g2_SendBuffer(&s_u8g2);  // 发送缓冲区数据到 OLED
}

// 设置字体
void drv_oled_set_font(drv_oled_font_t font)
{
    u8g2_SetFont(&s_u8g2, s_fonts[font]);  // 设置字体

    FontWidth  = (uint8_t)u8g2_GetMaxCharWidth(&s_u8g2);  // 获取最大字符宽度
    FontHeight = (uint8_t)u8g2_GetMaxCharHeight(&s_u8g2); // 获取最大字符高度
    FontAscent = (uint8_t)u8g2_GetAscent(&s_u8g2);        // 获取字体上移量
}

// 设置对比度
void drv_oled_set_contrast(uint8_t value)
{
    OLED_GUARD();

    u8g2_SetContrast(&s_u8g2, value);  // 设置对比度
    s_contrast = value;
}

// 获取对比度
uint8_t drv_oled_get_contrast(void)
{
    return s_contrast;
}

// 打印字符串
void drv_oled_print(uint8_t Line, uint8_t Column, const char *String)
{
    OLED_GUARD();

    // 检查参数是否有效
    if (String == NULL || FontWidth == 0 || FontHeight == 0)
    {
        return;
    }
    // 检查行和列是否有效
    if (Line < 1 || Line > drv_oled_max_line() ||
        Column < 1 || Column > drv_oled_max_col())
    {
        return;
    }

    uint8_t x = (uint8_t)((Column - 1) * FontWidth);             // 计算 x 坐标
    uint8_t y = (uint8_t)((Line - 1) * FontHeight + FontAscent); // 计算 y 坐标

    u8g2_DrawUTF8(&s_u8g2, x, y, String);
}

// 打印字符串（像素）
void drv_oled_text(uint8_t x, uint8_t y, const char *String)
{
    OLED_GUARD();

    if (String == NULL || FontWidth == 0 || FontHeight == 0)
    {
        return;
    }

    u8g2_DrawUTF8(&s_u8g2, x, y, String);
}

// 打印格式化字符串
void drv_oled_printf(uint8_t Line, uint8_t Column, const char *fmt, ...)
{
    if (fmt == NULL)
    {
        return;
    }

    char str[64];                           // 格式化字符串缓冲区
    va_list args;                           // 可变参数列表
    va_start(args, fmt);                    // 初始化可变参数列表
    vsnprintf(str, sizeof(str), fmt, args); // 格式化字符串
    va_end(args);                          // 结束可变参数列表

    drv_oled_print(Line, Column, str);
}

// 打印格式化字符串（像素）
void drv_oled_textf(uint8_t x, uint8_t y, const char *fmt, ...)
{
    if (fmt == NULL)
    {
        return;
    }

    char str[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);

    drv_oled_text(x, y, str);
}

// 打印数字
void drv_oled_print_num(uint8_t Line, uint8_t Column, float Num,
                    uint8_t DecimalPlaces, uint8_t ShowSign)
{
    char str[20];  // 格式化字符串缓冲区

    if (ShowSign)  // 是否显示符号
    {
        snprintf(str, sizeof(str), "%+.*f", DecimalPlaces, Num);  // 格式化符号
    }
    else
    {
        snprintf(str, sizeof(str), "%.*f", DecimalPlaces, Num);  // 格式化无符号
    }

    drv_oled_print(Line, Column, str);
}

// 打印数字（像素）
void drv_oled_text_num(uint8_t x, uint8_t y, float Num,
                       uint8_t DecimalPlaces, uint8_t ShowSign)
{
    char str[20];

    if (ShowSign)
    {
        snprintf(str, sizeof(str), "%+.*f", DecimalPlaces, Num);
    }
    else
    {
        snprintf(str, sizeof(str), "%.*f", DecimalPlaces, Num);
    }

    drv_oled_text(x, y, str);
}

// 绘制像素点
void drv_oled_draw_pixel(uint8_t x, uint8_t y)
{
    OLED_GUARD();
    if (x >= s_width || y >= s_height)  // 检查坐标是否有效
    {
        return;
    }
    u8g2_DrawPixel(&s_u8g2, x, y);
}

// 绘制线
void drv_oled_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    OLED_GUARD();
    if (x1 >= s_width || y1 >= s_height || x2 >= s_width || y2 >= s_height)  // 检查坐标是否有效
    {
        return;
    }
    u8g2_DrawLine(&s_u8g2, x1, y1, x2, y2);
}

// 绘制框
void drv_oled_draw_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    OLED_GUARD();
    if (w == 0 || h == 0)  // 检查参数是否有效
    {
        return;
    }
    if ((uint16_t)x + w > s_width || (uint16_t)y + h > s_height)  // 检查坐标是否有效
    {
        return;
    }
    u8g2_DrawFrame(&s_u8g2, x, y, w, h);
}

// 绘制矩形
void drv_oled_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    OLED_GUARD();
    if (w == 0 || h == 0)  // 检查参数是否有效
    {
        return;
    }
    if ((uint16_t)x + w > s_width || (uint16_t)y + h > s_height)  // 检查坐标是否有效
    {
        return;
    }
    u8g2_DrawBox(&s_u8g2, x, y, w, h);  
}

// 绘制空心圆
void drv_oled_draw_circle(uint8_t x, uint8_t y, uint8_t r)
{
    OLED_GUARD();
    if (r == 0 || (uint16_t)x + r > s_width || (uint16_t)y + r > s_height)  // 检查参数是否有效
    {
        return;
    }
    u8g2_DrawCircle(&s_u8g2, x, y, r, U8G2_DRAW_ALL);
}

// 绘制实心圆
void drv_oled_draw_disc(uint8_t x, uint8_t y, uint8_t r)
{
    OLED_GUARD();
    if (r == 0 || (uint16_t)x + r > s_width || (uint16_t)y + r > s_height)  // 检查参数是否有效
    {
        return;
    }
    u8g2_DrawDisc(&s_u8g2, x, y, r, U8G2_DRAW_ALL);
}

// 绘制位图
void drv_oled_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap)
{
    OLED_GUARD();
    if (bitmap == NULL || w == 0 || h == 0)  // 检查参数是否有效
    {
        return;
    }
    if ((uint16_t)x + w > s_width || (uint16_t)y + h > s_height)  // 检查坐标是否有效
    {
        return;
    }
    u8g2_DrawBitmap(&s_u8g2, x, y, w, h, bitmap); 
}

// 镜像
void drv_oled_mirror(uint8_t H, uint8_t V)
{
    OLED_GUARD();

    u8x8_t *u8x8 = u8g2_GetU8x8(&s_u8g2);  // 获取 u8x8 指针

    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, (H == 1) ? 0xA1 : 0xA0);   // 0xA1 左右镜像
    u8x8_cad_SendCmd(u8x8, (V == 1) ? 0xC0 : 0xC8);   // 0xC0 上下镜像
    u8x8_cad_EndTransfer(u8x8);
}

