#include "pwm_hal.h"

#include <stdio.h>
#include "driver/ledc.h"
#include "soc/soc_caps.h"

// 每个 PWM 通道的最大数量
#define HAL_PWM_MAX       SOC_LEDC_CHANNEL_NUM  

// PWM 时钟频率，单位：兹
#define HAL_PWM_CLK_HZ    80000000UL
// PWM 职空比，单位：千分比
#define HAL_PWM_DUTY_FULL 1000
// PWM 分辨率，单位：位
#define HAL_PWM_RES_MAX   SOC_LEDC_TIMER_BIT_WIDTH

// PWM 通道信息数组
static hal_pwm_info_t s_pwm[HAL_PWM_MAX]; 

// 按引脚查找 PWM 通道槽位，找不到返回 -1
static int hal_pwm_find(int pin)
{
    // 遍历所有 PWM 通道
    for (int i = 0; i < HAL_PWM_MAX; i++)
    {
        // 检查是否被使用
        if (s_pwm[i].used && s_pwm[i].pin == pin)
        {
            return i;
        }
    }
    return -1;
}

// 分配一个 PWM 通道槽位，找不到返回 -1
static int hal_pwm_alloc(void)
{
    for (int i = 0; i < HAL_PWM_MAX; i++)
    {
        // 检查是否被使用
        if (!s_pwm[i].used)
        {
            return i;
        }
    }
    return -1;
}

// 根据频率选择合适的分辨率，返回分辨率值
static uint8_t hal_pwm_pick_res(uint32_t freq_hz)
{
    // 选择默认分辨率 1 位
    uint8_t res = 1;

    // 计算分辨率：freq_hz * (2^res) <= HAL_PWM_CLK_HZ
    for (uint8_t r = 1; r <= HAL_PWM_RES_MAX; r++)
    {
        // 检查是否超过最大时钟频率
        if (((uint64_t)freq_hz << r) > HAL_PWM_CLK_HZ)
        {
            break;
        }
        // 记录当前分辨率
        res = r;
    }
    return res;
}

// 根据占空比和分辨率计算原始占空比值
static uint32_t hal_pwm_duty_raw(uint32_t permille, uint8_t res)
{
    // 检查占空比是否超出范围
    if (permille > HAL_PWM_DUTY_FULL)
    {
        // 超出范围则设置为最大值
        permille = HAL_PWM_DUTY_FULL;
    }

    // 计算最大值：2^res - 1
    uint32_t max = (1u << res) - 1u;
    // 计算原始占空比值：max * permille / HAL_PWM_DUTY_FULL
    return (uint32_t)(((uint64_t)max * permille) / HAL_PWM_DUTY_FULL);
}

static bool hal_pwm_apply_timer(hal_pwm_info_t* p, uint32_t freq_hz)
{
    // 选择合适的分辨率
    uint8_t res = hal_pwm_pick_res(freq_hz);

    ledc_timer_config_t tcfg =
    {
        .speed_mode       = LEDC_LOW_SPEED_MODE,    // 低速模式
        .duty_resolution  = (ledc_timer_bit_t)res,  // 分辨率，单位：位
        .timer_num        = (ledc_timer_t)p->timer, // 定时器编号
        .freq_hz          = freq_hz,                // 频率，单位：兹
        .clk_cfg          = LEDC_AUTO_CLK,          // 自动选择时钟源
    };

    // 配置 LEDC 定时器
    if (ledc_timer_config(&tcfg) != ESP_OK)
    {
        printf("pwm_hal: timer config failed\n");
        return false;
    }

    p -> resolution = res; // 保存分辨率
    p -> freq_hz    = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)p->timer); // 获取实际频率

    // 计算原始占空比值
    uint32_t raw = hal_pwm_duty_raw(p -> duty_permille, res);

    // 设置占空比
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)p->channel, raw) != ESP_OK)
    {
        return false;
    }

    // 更新占空比
    return (ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)p->channel) == ESP_OK);
}

bool hal_pwm_init(const hal_pwm_timer_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        printf("pwm_hal: cfg is NULL\n");
        return false;
    }

    int      pin = cfg->pin;
    uint32_t freq_hz = cfg->freq_hz;
    uint32_t duty_permille = cfg->duty_permille;

    // 检查频率是否在范围内
    if (freq_hz == 0 || freq_hz > 1000000)
    {
        printf("pwm_hal: freq %lu out of range (1 ~ 1000000)\n", (unsigned long)freq_hz);
        return false;
    }

    // 检查占空比是否在范围内
    if (duty_permille > HAL_PWM_DUTY_FULL)
    {
        printf("pwm_hal: duty %lu out of range (0 ~ %d)\n",
               (unsigned long)duty_permille, HAL_PWM_DUTY_FULL);
        return false;
    }

    /* 幂等：这个引脚已经配过就直接成功 */
    if (hal_pwm_find(pin) >= 0)
    {
        return true;
    }

    // 分配一个 PWM 通道槽位，找不到返回 false
    int slot = hal_pwm_alloc();
    // 检查是否成功分配槽位
    if (slot < 0)
    {
        printf("pwm_hal: no free timer (max %d channels)\n", HAL_PWM_MAX);
        return false;
    }

    s_pwm[slot].pin            = pin;           // 记录引脚号
    s_pwm[slot].timer          = (uint8_t)slot; // 记录定时器编号
    s_pwm[slot].channel        = (uint8_t)slot; // 记录通道编号
    s_pwm[slot].duty_permille  = duty_permille; // 记录占空比
    s_pwm[slot].used           = true;          // 标记为已使用

    // 应用定时器配置
    if (!hal_pwm_apply_timer(&s_pwm[slot], freq_hz))
    {
        s_pwm[slot].used = false;      /* 失败就退回，不留假账 */
        return false;
    }

    // 配置 LEDC 通道
    ledc_channel_config_t ccfg = 
    {
        .gpio_num   = pin,                                                      // 引脚号
        .speed_mode = LEDC_LOW_SPEED_MODE,                                      // 低速模式
        .channel    = (ledc_channel_t)s_pwm[slot].channel,                      // 通道编号
        .intr_type  = LEDC_INTR_DISABLE,                                        // 禁用中断
        .timer_sel  = (ledc_timer_t)s_pwm[slot].timer,                          // 定时器编号
        .duty       = hal_pwm_duty_raw(duty_permille, s_pwm[slot].resolution),  // 占空比值
        .hpoint     = 0,                                          // 高电平时间点，单位：微秒
    };

    // 配置 LEDC 通道
    if (ledc_channel_config(&ccfg) != ESP_OK)
    {
        printf("pwm_hal: channel config failed\n");
        return false;
    }

    return true;
}

// 设置 PWM 频率
bool hal_pwm_set_freq(int pin, uint32_t freq_hz)
{
    if (freq_hz == 0 || freq_hz > 1000000)
    {
        printf("pwm_hal: freq %lu out of range\n", (unsigned long)freq_hz);
        return false;
    } 

    int slot = hal_pwm_find(pin); 
    if (slot < 0)
    {
        printf("pwm_hal: pin %d not inited\n", pin);
        return false;
    }

    // 应用定时器配置
    return hal_pwm_apply_timer(&s_pwm[slot], freq_hz);
}

// 设置 PWM 占空比
bool hal_pwm_set_duty(int pin, uint32_t duty_permille)
{
    // 检查占空比是否在范围内
    if (duty_permille > HAL_PWM_DUTY_FULL)
    {
        // 超出范围，设置为最大占空比
        duty_permille = HAL_PWM_DUTY_FULL;  
    }

    int slot = hal_pwm_find(pin); 
    if (slot < 0)
    {
        printf("pwm_hal: pin %d not inited\n", pin);
        return false;
    }

    // 计算原始占空比值
    uint32_t raw = hal_pwm_duty_raw(duty_permille, s_pwm[slot].resolution);

    // 设置占空比
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)s_pwm[slot].channel, raw) != ESP_OK)
    {
        return false;
    }
    // 更新占空比
    if (ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)s_pwm[slot].channel) != ESP_OK)
    {
        return false;
    }

    s_pwm[slot].duty_permille = duty_permille; // 更新占空比
    return true;
}

// 更新 PWM 频率和占空比
bool hal_pwm_update(int pin, uint32_t freq_hz, uint32_t duty_permille)
{
    // 检查占空比是否在范围内
    if (duty_permille > HAL_PWM_DUTY_FULL)
    {
        duty_permille = HAL_PWM_DUTY_FULL;  
    }

    int slot = hal_pwm_find(pin); 
    if (slot < 0)
    {
        printf("pwm_hal: pin %d not inited\n", pin);
        return false;
    }

    // 检查频率是否在范围内
    if (freq_hz == 0 || freq_hz > 1000000)
    {
        return false;
    }

    s_pwm[slot].duty_permille = duty_permille; // 更新占空比
    // 应用定时器配置
    return hal_pwm_apply_timer(&s_pwm[slot], freq_hz);
}

// 停止 PWM 通道
bool hal_pwm_stop(int pin)
{
    int slot = hal_pwm_find(pin); 
    if (slot < 0)
    {
        return true;        /* 本来就没在跑 */
    }

    // 停止 PWM 通道
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)s_pwm[slot].channel, 0);

    s_pwm[slot].used           = false;  // 标记为未使用
    s_pwm[slot].pin            = -1;     // 重置引脚号
    s_pwm[slot].duty_permille  = 0;      // 重置占空比
    s_pwm[slot].freq_hz        = 0;      // 重置频率
    return true;
}

// 获取 PWM 通道
bool hal_pwm_get_info(int pin, hal_pwm_info_t *out)
{
    // 检查输出指针是否为空
    if (out == NULL)
    {
        printf("pwm_hal: out is NULL\n");
        return false;
    }

    int slot = hal_pwm_find(pin);
    if (slot < 0)
    {
        printf("pwm_hal: pin %d not inited\n", pin);
        return false;
    }

    *out = s_pwm[slot];     /* 整个结构体拷出去 */
    return true;
}

