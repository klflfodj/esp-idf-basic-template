#include "eye_expression.h"

#include <stdint.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_random.h"

#include "oled_drv.h"


#define EYE_FB_SIZE          1024
#define EYE_TRANSITION_MS    600
#define EYE_LOOK_MS          500


// ========== 帧缓冲 ========== //
// SSD1306 页格式：8 页 × 128 字节，bit0 在上

static uint8_t s_fb[EYE_FB_SIZE];


// ========== 时间与随机 ========== //

static uint32_t eye_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static int eye_random(int min, int max)
{
    if (max <= min)
    {
        return min;
    }

    return min + (int)(esp_random() % (uint32_t)(max - min));
}


// ========== 帧缓冲操作 ========== //

static void fb_clear(void)
{
    memset(s_fb, 0, EYE_FB_SIZE);
}

static void fb_set_pixel(int16_t x, int16_t y, uint8_t color)
{
    if ((x < 0) || (x >= 128) || (y < 0) || (y >= 64))
    {
        return;
    }

    uint16_t idx = ((uint16_t)(y >> 3) << 7) + (uint16_t)x;
    uint8_t  bit = (uint8_t)(1u << (y & 0x07));

    if (color)
    {
        s_fb[idx] |= bit;
    }
    else
    {
        s_fb[idx] &= (uint8_t)~bit;
    }
}

static void eye_fb_flush(void)
{
    drv_oled_blit(s_fb);
    drv_oled_update();
}


// ========== 画图原语 ========== //

static void fb_draw_hline(int16_t x, int16_t y, int16_t len)
{
    for (int16_t i = 0; i < len; i++)
    {
        fb_set_pixel(x + i, y, 1);
    }
}

static void fb_draw_rect(int16_t l, int16_t t, int16_t r, int16_t b)
{
    if (l > r) { int16_t tmp = l; l = r; r = tmp; }
    if (t > b) { int16_t tmp = t; t = b; b = tmp; }

    for (int16_t y = t; y <= b; y++)
    {
        for (int16_t x = l; x <= r; x++)
        {
            fb_set_pixel(x, y, 1);
        }
    }
}

static void fb_draw_triangle(int16_t x0, int16_t y0,
                             int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2)
{
    if (y0 > y1) { int16_t t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y1 > y2) { int16_t t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y0 > y1) { int16_t t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }

    if (y2 <= y0)
    {
        return;
    }

    for (int16_t y = y0; y < y2; y++)
    {
        int16_t x_left;
        int16_t x_right;

        if (y < y1)
        {
            float t0 = (y1 == y0) ? 0.5f : (float)(y - y0) / (y1 - y0);
            float t1 = (y2 == y0) ? 0.5f : (float)(y - y0) / (y2 - y0);

            x_left  = (int16_t)(x0 + (x1 - x0) * t0);
            x_right = (int16_t)(x0 + (x2 - x0) * t1);
        }
        else
        {
            float t0 = (y2 == y1) ? 0.5f : (float)(y - y1) / (y2 - y1);
            float t1 = (y2 == y0) ? 0.5f : (float)(y - y0) / (y2 - y0);

            x_left  = (int16_t)(x1 + (x2 - x1) * t0);
            x_right = (int16_t)(x0 + (x2 - x0) * t1);
        }

        if (x_left > x_right)
        {
            int16_t t = x_left; x_left = x_right; x_right = t;
        }

        fb_draw_hline(x_left, y, (int16_t)(x_right - x_left + 1));
    }
}

static void fb_draw_right_triangle(int16_t x0, int16_t y0,
                                   int16_t x1, int16_t y1)
{
    fb_draw_triangle(x0, y0, x1, y1, x1, y0);
}


// ========== 填充四分之一椭圆 ========== //

typedef enum
{
    EC_TR = 0,
    EC_TL,
    EC_BR,
    EC_BL,
} eye_corner_t;

static void fb_fill_ellipse_corner(eye_corner_t corner,
                                   int16_t x0, int16_t y0,
                                   int32_t rx, int32_t ry)
{
    if ((rx < 2) || (ry < 2))
    {
        return;
    }

    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t fx2 = 4 * rx2;
    int32_t fy2 = 4 * ry2;
    int32_t x, y, s;

    switch (corner)
    {
        case EC_TR:
            for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++)
            {
                fb_draw_hline(x0, (int16_t)(y0 - y), (int16_t)x);
                if (s >= 0) { s += fx2 * (1 - y); y--; }
                s += ry2 * ((4 * x) + 6);
            }
            for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++)
            {
                fb_draw_hline(x0, (int16_t)(y0 - y), (int16_t)x);
                if (s >= 0) { s += fy2 * (1 - x); x--; }
                s += rx2 * ((4 * y) + 6);
            }
            break;

        case EC_BR:
            for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++)
            {
                fb_draw_hline(x0, (int16_t)(y0 + y - 1), (int16_t)x);
                if (s >= 0) { s += fx2 * (1 - y); y--; }
                s += ry2 * ((4 * x) + 6);
            }
            for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++)
            {
                fb_draw_hline(x0, (int16_t)(y0 + y - 1), (int16_t)x);
                if (s >= 0) { s += fy2 * (1 - x); x--; }
                s += rx2 * ((4 * y) + 6);
            }
            break;

        case EC_TL:
            for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++)
            {
                fb_draw_hline((int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)x);
                if (s >= 0) { s += fx2 * (1 - y); y--; }
                s += ry2 * ((4 * x) + 6);
            }
            for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++)
            {
                fb_draw_hline((int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)x);
                if (s >= 0) { s += fy2 * (1 - x); x--; }
                s += rx2 * ((4 * y) + 6);
            }
            break;

        case EC_BL:
            for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++)
            {
                fb_draw_hline((int16_t)(x0 - x), (int16_t)(y0 + y - 1), (int16_t)x);
                if (s >= 0) { s += fx2 * (1 - y); y--; }
                s += ry2 * ((4 * x) + 6);
            }
            for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++)
            {
                fb_draw_hline((int16_t)(x0 - x), (int16_t)(y0 + y), (int16_t)x);
                if (s >= 0) { s += fy2 * (1 - x); x--; }
                s += rx2 * ((4 * y) + 6);
            }
            break;

        default:
            break;
    }
}


// ========== 眼型配置与预设 ========== //

typedef struct
{
    int16_t OffsetX;
    int16_t OffsetY;
    int16_t Height;
    int16_t Width;
    float   Slope_Top;
    float   Slope_Bottom;
    int16_t Radius_Top;
    int16_t Radius_Bottom;
    int16_t Inverse_Radius_Top;      // 未使用，保留与原版一致
    int16_t Inverse_Radius_Bottom;   // 未使用
    int16_t Inverse_Offset_Top;      // 未使用
    int16_t Inverse_Offset_Bottom;   // 未使用
} EyeConfig;

static const EyeConfig EyePreset_Normal = { 0,0, 40,40, 0,0, 8,8, 0,0,0,0 };
static const EyeConfig EyePreset_Happy = { 0,0, 10,40, 0,0, 10,0, 0,0,0,0 };
static const EyeConfig EyePreset_Glee = { 0,0, 8,40, 0,0, 8,0, 0,5,0,0 };
static const EyeConfig EyePreset_Sad = { 0,0, 15,40, -0.5f,0, 1,10, 0,0,0,0 };
static const EyeConfig EyePreset_Worried = { 0,0, 25,40, -0.1f,0, 6,10, 0,0,0,0 };
static const EyeConfig EyePreset_Worried_Alt = { 0,0, 35,40, -0.2f,0, 6,10, 0,0,0,0 };
static const EyeConfig EyePreset_Focused = { 0,0, 14,40, 0.2f,0, 3,1, 0,0,0,0 };
static const EyeConfig EyePreset_Annoyed = { 0,0, 12,40, 0,0, 0,10, 0,0,0,0 };
static const EyeConfig EyePreset_Annoyed_Alt = { 0,0, 5,40, 0,0, 0,4, 0,0,0,0 };
static const EyeConfig EyePreset_Surprised = { -2,0, 45,45, 0,0, 16,16, 0,0,0,0 };
static const EyeConfig EyePreset_Skeptic = { 0,0, 40,40, 0,0, 10,10, 0,0,0,0 };
static const EyeConfig EyePreset_Skeptic_Alt = { 0,-6, 26,40, 0.3f,0, 1,10, 0,0,0,0 };
static const EyeConfig EyePreset_Frustrated = { 3,-5, 12,40, 0,0, 0,10, 0,0,0,0 };
static const EyeConfig EyePreset_Unimpressed = { 3,0, 12,40, 0,0, 1,10, 0,0,0,0 };
static const EyeConfig EyePreset_Unimpressed_Alt = { 3,-3, 22,40, 0,0, 1,16, 0,0,0,0 };
static const EyeConfig EyePreset_Sleepy = { 0,-2, 14,40, -0.5f, -0.5f, 3,3, 0,0,0,0 };
static const EyeConfig EyePreset_Sleepy_Alt = { 0,-2, 8,40, -0.5f, -0.5f, 3,3, 0,0,0,0 };
static const EyeConfig EyePreset_Suspicious = { 0,0, 22,40, 0,0, 8,3, 0,0,0,0 };
static const EyeConfig EyePreset_Suspicious_Alt = { 0,-3, 16,40, 0.2f,0, 6,3, 0,0,0,0 };
static const EyeConfig EyePreset_Squint = { -10,-3, 35,35, 0,0, 8,8, 0,0,0,0 };
static const EyeConfig EyePreset_Squint_Alt = { 5,0, 20,20, 0,0, 5,5, 0,0,0,0 };
static const EyeConfig EyePreset_Angry = { -3,0, 20,40, 0.3f,0, 2,12, 0,0,0,0 };
static const EyeConfig EyePreset_Furious = { -2,0, 30,40, 0.4f,0, 2,8, 0,0,0,0 };
static const EyeConfig EyePreset_Scared = { -3,0, 40,40, -0.1f,0, 12,8, 0,0,0,0 };
static const EyeConfig EyePreset_Awe = { 2,0, 35,45, -0.1f, 0.1f, 12,12, 0,0,0,0 };


// ========== 单只眼绘制 ========== //

static void eye_draw(int16_t centerX, int16_t centerY, const EyeConfig *cfg)
{
    EyeConfig c = *cfg;

    int32_t delta_y_top    = (int32_t)(c.Height * c.Slope_Top / 2.0f);
    int32_t delta_y_bottom = (int32_t)(c.Height * c.Slope_Bottom / 2.0f);
    int32_t totalHeight    = c.Height + delta_y_top - delta_y_bottom;

    if ((c.Radius_Bottom > 0) && (c.Radius_Top > 0) &&
        (totalHeight - 1 < c.Radius_Bottom + c.Radius_Top))
    {
        int32_t crt = (int32_t)((float)c.Radius_Top * (totalHeight - 1)
                                / (c.Radius_Bottom + c.Radius_Top));
        int32_t crb = (int32_t)((float)c.Radius_Bottom * (totalHeight - 1)
                                / (c.Radius_Bottom + c.Radius_Top));

        c.Radius_Top    = (int16_t)((crt >= 0) ? crt : 0);
        c.Radius_Bottom = (int16_t)((crb >= 0) ? crb : 0);
    }

    int32_t TLc_y = centerY + c.OffsetY - c.Height / 2 + c.Radius_Top - delta_y_top;
    int32_t TLc_x = centerX + c.OffsetX - c.Width / 2 + c.Radius_Top;
    int32_t TRc_y = centerY + c.OffsetY - c.Height / 2 + c.Radius_Top + delta_y_top;
    int32_t TRc_x = centerX + c.OffsetX + c.Width / 2 - c.Radius_Top;
    int32_t BLc_y = centerY + c.OffsetY + c.Height / 2 - c.Radius_Bottom - delta_y_bottom;
    int32_t BLc_x = centerX + c.OffsetX - c.Width / 2 + c.Radius_Bottom;
    int32_t BRc_y = centerY + c.OffsetY + c.Height / 2 - c.Radius_Bottom + delta_y_bottom;
    int32_t BRc_x = centerX + c.OffsetX + c.Width / 2 - c.Radius_Bottom;

    int32_t min_c_x = (TLc_x < BLc_x) ? TLc_x : BLc_x;
    int32_t max_c_x = (TRc_x > BRc_x) ? TRc_x : BRc_x;
    int32_t min_c_y = (TLc_y < TRc_y) ? TLc_y : TRc_y;
    int32_t max_c_y = (BLc_y > BRc_y) ? BLc_y : BRc_y;

    fb_draw_rect((int16_t)min_c_x, (int16_t)min_c_y,
                 (int16_t)max_c_x, (int16_t)max_c_y);

    fb_draw_rect((int16_t)TRc_x, (int16_t)TRc_y,
                 (int16_t)(BRc_x + c.Radius_Bottom), (int16_t)BRc_y);
    fb_draw_rect((int16_t)(TLc_x - c.Radius_Top), (int16_t)TLc_y,
                 (int16_t)BLc_x, (int16_t)BLc_y);
    fb_draw_rect((int16_t)TLc_x, (int16_t)(TLc_y - c.Radius_Top),
                 (int16_t)TRc_x, (int16_t)TRc_y);
    fb_draw_rect((int16_t)BLc_x, (int16_t)BLc_y,
                 (int16_t)BRc_x, (int16_t)(BRc_y + c.Radius_Bottom));

    if (c.Slope_Top > 0)
    {
        fb_draw_right_triangle((int16_t)TLc_x, (int16_t)(TLc_y - c.Radius_Top),
                               (int16_t)TRc_x, (int16_t)(TRc_y - c.Radius_Top + 1));
    }
    else if (c.Slope_Top < 0)
    {
        fb_draw_right_triangle((int16_t)TRc_x, (int16_t)(TRc_y - c.Radius_Top),
                               (int16_t)TLc_x, (int16_t)(TLc_y - c.Radius_Top + 1));
    }

    if (c.Slope_Bottom > 0)
    {
        fb_draw_right_triangle((int16_t)(BRc_x + c.Radius_Bottom),
                               (int16_t)(BRc_y + c.Radius_Bottom),
                               (int16_t)(BLc_x - c.Radius_Bottom + 1),
                               (int16_t)(BLc_y + c.Radius_Bottom));
    }
    else if (c.Slope_Bottom < 0)
    {
        fb_draw_right_triangle((int16_t)(BLc_x - c.Radius_Bottom + 1),
                               (int16_t)(BLc_y + c.Radius_Bottom),
                               (int16_t)(BRc_x + c.Radius_Bottom),
                               (int16_t)(BRc_y + c.Radius_Bottom));
    }

    if (c.Radius_Top > 0)
    {
        fb_fill_ellipse_corner(EC_TL, (int16_t)TLc_x, (int16_t)TLc_y, c.Radius_Top, c.Radius_Top);
        fb_fill_ellipse_corner(EC_TR, (int16_t)TRc_x, (int16_t)TRc_y, c.Radius_Top, c.Radius_Top);
    }

    if (c.Radius_Bottom > 0)
    {
        fb_fill_ellipse_corner(EC_BL, (int16_t)BLc_x, (int16_t)BLc_y, c.Radius_Bottom, c.Radius_Bottom);
        fb_fill_ellipse_corner(EC_BR, (int16_t)BRc_x, (int16_t)BRc_y, c.Radius_Bottom, c.Radius_Bottom);
    }
}


// ========== 配置插值 ========== //

static EyeConfig lerp_config(const EyeConfig *a, const EyeConfig *b, float t)
{
    EyeConfig r;

    r.OffsetX   = (int16_t)(a->OffsetX + (b->OffsetX - a->OffsetX) * t);
    r.OffsetY   = (int16_t)(a->OffsetY + (b->OffsetY - a->OffsetY) * t);
    r.Height    = (int16_t)(a->Height + (b->Height - a->Height) * t);
    r.Width     = (int16_t)(a->Width + (b->Width - a->Width) * t);

    r.Slope_Top    = a->Slope_Top + (b->Slope_Top - a->Slope_Top) * t;
    r.Slope_Bottom = a->Slope_Bottom + (b->Slope_Bottom - a->Slope_Bottom) * t;

    r.Radius_Top    = (int16_t)(a->Radius_Top + (b->Radius_Top - a->Radius_Top) * t);
    r.Radius_Bottom = (int16_t)(a->Radius_Bottom + (b->Radius_Bottom - a->Radius_Bottom) * t);

    r.Inverse_Radius_Top    = (int16_t)(a->Inverse_Radius_Top + (b->Inverse_Radius_Top - a->Inverse_Radius_Top) * t);
    r.Inverse_Radius_Bottom = (int16_t)(a->Inverse_Radius_Bottom + (b->Inverse_Radius_Bottom - a->Inverse_Radius_Bottom) * t);
    r.Inverse_Offset_Top    = (int16_t)(a->Inverse_Offset_Top + (b->Inverse_Offset_Top - a->Inverse_Offset_Top) * t);
    r.Inverse_Offset_Bottom = (int16_t)(a->Inverse_Offset_Bottom + (b->Inverse_Offset_Bottom - a->Inverse_Offset_Bottom) * t);

    return r;
}


// ========== 眼睛与脸的状态 ========== //

typedef struct
{
    EyeConfig     config;
    EyeConfig     target_config;
    uint32_t      transition_start;
    bool          is_left;

    float         blink_t;
    uint32_t      blink_start;
    uint32_t      blink_duration;
    bool          blink_pending;

    float         move_x;
    float         move_y;
    float         target_move_x;
    float         target_move_y;
    uint32_t      look_start;

    int16_t       cx;
    int16_t       cy;
} EyeState;

typedef struct
{
    EyeState     left;
    EyeState     right;

    eye_emotion_t current_emotion;

    bool          random_blink;
    bool          random_behavior;
    bool          random_look;

    uint32_t      next_blink_time;
    uint32_t      next_emotion_time;
    uint32_t      next_look_time;
} FaceState;

static FaceState s_face;


typedef struct
{
    const EyeConfig *left;
    const EyeConfig *right;
} PresetPair;

static const PresetPair g_presets[EYE_EMOTION_COUNT] =
{
    { &EyePreset_Normal,      &EyePreset_Normal      },
    { &EyePreset_Angry,       &EyePreset_Angry       },
    { &EyePreset_Glee,        &EyePreset_Glee        },
    { &EyePreset_Happy,       &EyePreset_Happy       },
    { &EyePreset_Sad,         &EyePreset_Sad         },
    { &EyePreset_Worried,     &EyePreset_Worried_Alt },
    { &EyePreset_Focused,     &EyePreset_Focused     },
    { &EyePreset_Annoyed,     &EyePreset_Annoyed_Alt },
    { &EyePreset_Surprised,   &EyePreset_Surprised   },
    { &EyePreset_Skeptic,     &EyePreset_Skeptic_Alt },
    { &EyePreset_Frustrated,  &EyePreset_Frustrated  },
    { &EyePreset_Unimpressed, &EyePreset_Unimpressed_Alt },
    { &EyePreset_Sleepy,      &EyePreset_Sleepy_Alt  },
    { &EyePreset_Suspicious,  &EyePreset_Suspicious_Alt },
    { &EyePreset_Squint,      &EyePreset_Squint_Alt  },
    { &EyePreset_Furious,     &EyePreset_Furious     },
    { &EyePreset_Scared,      &EyePreset_Scared      },
    { &EyePreset_Awe,         &EyePreset_Awe         },
};


// ========== 单眼动画 ========== //

static EyeConfig mirror_config(const EyeConfig *src)
{
    EyeConfig c = *src;

    c.OffsetX      = (int16_t)-c.OffsetX;
    c.Slope_Top    = -c.Slope_Top;
    c.Slope_Bottom = -c.Slope_Bottom;

    return c;
}

static void eye_set_target(EyeState *eye, const EyeConfig *preset)
{
    eye->target_config    = eye->is_left ? mirror_config(preset) : *preset;
    eye->transition_start = eye_millis();
}

static void eye_update_transition(EyeState *eye)
{
    uint32_t elapsed = eye_millis() - eye->transition_start;
    float    t = (elapsed >= EYE_TRANSITION_MS) ? 1.0f
                 : (float)elapsed / EYE_TRANSITION_MS;

    eye->config = lerp_config(&eye->config, &eye->target_config, t);
}

static void eye_update_look(EyeState *eye)
{
    uint32_t elapsed = eye_millis() - eye->look_start;
    float    t = (elapsed >= EYE_LOOK_MS) ? 1.0f
                 : (float)elapsed / EYE_LOOK_MS;

    t = t * t * (3.0f - 2.0f * t);

    eye->move_x = eye->move_x + (eye->target_move_x - eye->move_x) * t;
    eye->move_y = eye->move_y + (eye->target_move_y - eye->move_y) * t;
}

static void eye_render(EyeState *eye)
{
    EyeConfig draw_cfg = eye->config;

    draw_cfg.OffsetX += (int16_t)eye->move_x;
    draw_cfg.OffsetY -= (int16_t)eye->move_y;

    if (eye->blink_t > 0.0f)
    {
        int16_t h  = draw_cfg.Height;
        int16_t w  = draw_cfg.Width;
        float   bt = eye->blink_t;

        if (bt > 1.0f)
        {
            bt = 1.0f;
        }

        draw_cfg.Height = (int16_t)(h * (1.0f - bt * 0.95f));
        if (draw_cfg.Height < 2)
        {
            draw_cfg.Height = 2;
        }

        draw_cfg.Width = (int16_t)(w * (1.0f + bt * 0.15f));
    }

    eye_draw(eye->cx, eye->cy, &draw_cfg);
}

static void eye_update_blink(EyeState *eye)
{
    if (!eye->blink_pending)
    {
        return;
    }

    uint32_t elapsed = eye_millis() - eye->blink_start;
    uint32_t dur     = eye->blink_duration;

    if (elapsed >= dur)
    {
        eye->blink_t       = 0.0f;
        eye->blink_pending = false;
        return;
    }

    float t = (float)elapsed / dur;

    if (t < 0.30f)
    {
        eye->blink_t = t / 0.30f;
    }
    else if (t < 0.35f)
    {
        eye->blink_t = 1.0f;
    }
    else
    {
        eye->blink_t = 1.0f - (t - 0.35f) / 0.65f;
    }

    if (eye->blink_t < 0.0f) { eye->blink_t = 0.0f; }
    if (eye->blink_t > 1.0f) { eye->blink_t = 1.0f; }
}

static void eye_trigger_blink(EyeState *eye, uint32_t duration_ms)
{
    eye->blink_start    = eye_millis();
    eye->blink_duration = duration_ms;
    eye->blink_pending  = true;
}

static void eye_set_look(EyeState *eye, float mx, float my)
{
    eye->target_move_x = eye->is_left ? -mx : mx;
    eye->target_move_y = my;
    eye->look_start    = eye_millis();
}


// ========== 公共接口 ========== //

void eye_expression_init(void)
{
    memset(&s_face, 0, sizeof(s_face));

    s_face.left.is_left  = true;
    s_face.right.is_left = false;

    s_face.left.cx  = 32;
    s_face.left.cy  = 32;
    s_face.right.cx = 96;
    s_face.right.cy = 32;

    s_face.random_blink    = true;
    s_face.random_look     = true;
    s_face.random_behavior = false;

    s_face.next_blink_time   = eye_millis() + (uint32_t)eye_random(5000, 12000);
    s_face.next_emotion_time = 0;
    s_face.next_look_time    = eye_millis() + (uint32_t)eye_random(3000, 7000);

    const PresetPair *pp = &g_presets[EYE_EMOTION_NORMAL];

    s_face.left.config        = s_face.left.is_left ? mirror_config(pp->left) : *pp->left;
    s_face.left.target_config = s_face.left.config;

    s_face.right.config        = s_face.right.is_left ? mirror_config(pp->right) : *pp->right;
    s_face.right.target_config = s_face.right.config;

    s_face.current_emotion = EYE_EMOTION_NORMAL;
}

void eye_expression_update(void)
{
    EyeState *left  = &s_face.left;
    EyeState *right = &s_face.right;
    uint32_t  now   = eye_millis();

    eye_update_transition(left);
    eye_update_transition(right);

    eye_update_look(left);
    eye_update_look(right);

    eye_update_blink(left);
    eye_update_blink(right);

    if (s_face.random_blink && (now >= s_face.next_blink_time))
    {
        eye_trigger_blink(left, 300);
        eye_trigger_blink(right, 300);
        s_face.next_blink_time = now + (uint32_t)eye_random(5000, 12000);
    }

    if (s_face.random_look && (now >= s_face.next_look_time))
    {
        float lx = (float)eye_random(-80, 80) / 100.0f;
        float ly = (float)eye_random(-60, 60) / 100.0f;

        eye_set_look(left, lx, ly);
        eye_set_look(right, lx, ly);

        s_face.next_look_time = now + (uint32_t)eye_random(5000, 12000);
    }

    if (s_face.random_behavior && (s_face.next_emotion_time > 0) &&
        (now >= s_face.next_emotion_time))
    {
        eye_emotion_t e = (eye_emotion_t)eye_random(0, (int)EYE_EMOTION_COUNT);

        eye_expression_set_emotion(e);
        s_face.next_emotion_time = now + (uint32_t)eye_random(5000, 12000);
    }

    fb_clear();
    eye_render(left);
    eye_render(right);
    eye_fb_flush();
}

void eye_expression_set_emotion(eye_emotion_t emotion)
{
    if ((emotion < 0) || (emotion >= EYE_EMOTION_COUNT))
    {
        return;
    }

    s_face.current_emotion = emotion;

    const PresetPair *pp = &g_presets[emotion];

    eye_set_target(&s_face.left, pp->left);
    eye_set_target(&s_face.right, pp->right);
}

void eye_expression_blink(void)
{
    eye_trigger_blink(&s_face.left, 150);
    eye_trigger_blink(&s_face.right, 150);
}

void eye_expression_look_at(float x, float y)
{
    if (x < -1.0f) { x = -1.0f; }
    if (x > 1.0f)  { x = 1.0f; }
    if (y < -1.0f) { y = -1.0f; }
    if (y > 1.0f)  { y = 1.0f; }

    eye_set_look(&s_face.left, x * 12.0f, y * 10.0f);
    eye_set_look(&s_face.right, x * 12.0f, y * 10.0f);
}

void eye_expression_set_random_behavior(bool enable)
{
    s_face.random_behavior = enable;

    if (enable)
    {
        s_face.next_emotion_time = eye_millis() + (uint32_t)eye_random(5000, 12000);
    }
    else
    {
        s_face.next_emotion_time = 0;
    }
}

void eye_expression_set_random_blink(bool enable)
{
    s_face.random_blink = enable;

    if (enable)
    {
        s_face.next_blink_time = eye_millis() + (uint32_t)eye_random(2000, 5000);
    }
}

void eye_expression_set_random_look(bool enable)
{
    s_face.random_look = enable;

    if (enable)
    {
        s_face.next_look_time = eye_millis() + (uint32_t)eye_random(2000, 4000);
    }
}

eye_emotion_t eye_expression_get_emotion(void)
{
    return s_face.current_emotion;
}
