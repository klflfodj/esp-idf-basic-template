#include "servo_drv.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pwm_hal.h"
#include "esp_log.h"

// PWM 频率，舵机固定 50Hz
#define SERVO_FREQ_HZ      50
// 一个周期的长度（微秒），50Hz = 20000us
#define SERVO_PERIOD_US    20000
// 角度上限
#define SERVO_ANGLE_MAX    180
// 中位角度，初始化时先到中位再平滑归零
#define SERVO_MID_ANGLE    90
// 平滑时的步进间隔（毫秒）
#define SERVO_STEP_MS      20


static const char *TAG = "servo_drv";


// 脉宽(us) -> PWM 占空比(万分比)
// 50Hz 下 1 个占空比单位 = 20000 / 10000 = 2us
static uint32_t servo_us_to_duty(uint16_t us)
{
    return (uint32_t)us * HAL_PWM_DUTY_MAX / SERVO_PERIOD_US;
}

// 转换角度为脉宽（微秒）
// 角度 -> 脉宽(us)，按配置的 min_us / max_us 线性映射
static uint16_t servo_angle_to_us(const drv_servo_t *s, uint8_t angle)
{
    return (uint16_t)(s->min_us +
           (uint32_t)(s->max_us - s->min_us) * angle / SERVO_ANGLE_MAX);
}

// 脉宽(us) -> 角度，超范围限幅
// 给 drv_servo_set_us() 用，保证 s->angle 记录的值始终合法
static uint8_t servo_us_to_angle(const drv_servo_t *s, uint16_t us)
{
    // 脉宽超出范围，直接返回角度上限
    if (us <= s->min_us)
    {
        return 0;
    }
    if (us >= s->max_us)
    {
        return SERVO_ANGLE_MAX;
    }

    return (uint8_t)((uint32_t)(us - s->min_us) * SERVO_ANGLE_MAX /
                     (s->max_us - s->min_us));
}

// 初始化舵机
bool drv_servo_init(drv_servo_t *s, const drv_servo_cfg_t *cfg)
{
    if (s == NULL || cfg == NULL)
    {
        ESP_LOGE(TAG, "init: parameter is NULL");
        return false;
    }

    // 幂等：同一个引脚已经初始化过就直接成功
    if (s->inited && s->pin == cfg->pin)
    {
        return true;
    }

    // 脉宽范围检查
    if (cfg->min_us >= cfg->max_us)
    {
        ESP_LOGE(TAG, "init: min_us %u >= max_us %u",
                 (unsigned)cfg->min_us, (unsigned)cfg->max_us);
        return false;
    }
    if (cfg->max_us > SERVO_PERIOD_US)
    {
        ESP_LOGE(TAG, "init: max_us %u > period %d",
                 (unsigned)cfg->max_us, SERVO_PERIOD_US);
        return false;
    }

    //    建 PWM，第一帧给中位
    //    给中位不给 0°,上电时舵机位置未知,中位最坏误差 90°，
    //    给 0° 最坏误差 180°
    uint16_t mid_us = (uint16_t)((cfg->min_us + cfg->max_us) / 2);

    //    配置 PWM 通道
    hal_pwm_timer_cfg_t pcfg =
    {
        .pin     = cfg->pin,
        .freq_hz = SERVO_FREQ_HZ,
        .duty    = servo_us_to_duty(mid_us),
    };

    if (!hal_pwm_init(&pcfg))
    {
        ESP_LOGE(TAG, "init: pwm init failed on pin %d", cfg->pin);
        return false;
    }

    // 记录初始化舵机状态
    s->pin     = cfg->pin;
    s->min_us  = cfg->min_us;
    s->max_us  = cfg->max_us;
    s->angle   = SERVO_MID_ANGLE;
    s->inited  = true;

    // 平滑归到 0°
    if (cfg->home_ms > 0)
    {
        if (!drv_servo_move(s, 0, cfg->home_ms))
        {
            ESP_LOGE(TAG, "init: homing failed");
            return false;
        }
    }
    else
    {
        if (!drv_servo_set_angle(s, 0))
        {
            ESP_LOGE(TAG, "init: set angle 0 failed");
            return false;
        }
    }

    ESP_LOGI(TAG, "init done: pin %d, %u~%uus, %ums homing",
             cfg->pin, (unsigned)cfg->min_us, (unsigned)cfg->max_us,
             (unsigned)cfg->home_ms);
    return true;
}

// 立刻转到指定角度 0~180（超过 180 会被拒绝，不截断）
bool drv_servo_set_angle(drv_servo_t *s, uint8_t angle)
{
    if (s == NULL || !s->inited)
    {
        ESP_LOGE(TAG, "set_angle: not inited");
        return false;
    }

    // 超过 180 直接拒绝，不截断
    // 截断会让"参数写错了"变成"舵机悄悄停在别处"
    if (angle > SERVO_ANGLE_MAX)
    {
        ESP_LOGE(TAG, "set_angle: %u > %d", (unsigned)angle, SERVO_ANGLE_MAX);
        return false;
    }

    uint16_t us = servo_angle_to_us(s, angle);

    if (!hal_pwm_set_duty(s->pin, servo_us_to_duty(us)))
    {
        ESP_LOGE(TAG, "set_angle: pwm set duty failed");
        return false;
    }

    s->angle = angle;

    return true;
}

// 平滑转到指定角度 0~180（超过 180 会被拒绝，不截断）
bool drv_servo_move(drv_servo_t *s, uint8_t target, uint16_t duration_ms)
{
    if (s == NULL || !s->inited)
    {
        ESP_LOGE(TAG, "move: not inited");
        return false;
    }

    if (target > SERVO_ANGLE_MAX)
    {
        ESP_LOGE(TAG, "move: target %u > %d", (unsigned)target, SERVO_ANGLE_MAX);
        return false;
    }

    uint8_t start = s->angle;

    // 时长太短或本来就在目标位置：直接到位
    if (duration_ms < SERVO_STEP_MS || start == target)
    {
        return drv_servo_set_angle(s, target);
    }

    // 计算每一步的角度变化
    uint16_t steps = duration_ms / SERVO_STEP_MS;

    // 每一步都从起点重新算，不要累加
    // 累加的话整数除法会把小步长变成 0，舵机就不动了
    for (uint16_t i = 1; i <= steps; i++)
    {
        // 计算当前角度
        int32_t a = (int32_t)start +
                    ((int32_t)target - (int32_t)start) * (int32_t)i / (int32_t)steps;

        // 设置当前角度
        if (!drv_servo_set_angle(s, (uint8_t)a))
        {
            // 中途失败：s->angle 停在上一步成功的位置
            ESP_LOGE(TAG, "move: failed at step %u", (unsigned)i);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_MS));
    }

    return true;
}

// 直接给脉宽（调试用）：用它可以精确测出这只舵机的真实范围
bool drv_servo_set_us(drv_servo_t *s, uint16_t us)
{
    if (s == NULL || !s->inited)
    {
        ESP_LOGE(TAG, "set_us: not inited");
        return false;
    }

    if (us > SERVO_PERIOD_US)
    {
        ESP_LOGE(TAG, "set_us: %u > period %d", (unsigned)us, SERVO_PERIOD_US);
        return false;
    }

    // 脉宽超出范围，直接返回角度上限
    if (us != 0 && (us < s->min_us || us > s->max_us))
    {
        ESP_LOGW(TAG, "set_us: %u outside %u~%u, may hit mechanical limit",
                 (unsigned)us, (unsigned)s->min_us, (unsigned)s->max_us);
    }

    if (!hal_pwm_set_duty(s -> pin, servo_us_to_duty(us)))
    {
        ESP_LOGE(TAG, "set_us: pwm set duty failed");
        return false;
    }

    s->angle = servo_us_to_angle(s, us);

    return true;
}

// 获取当前角度
bool drv_servo_get_angle(drv_servo_t *s, uint8_t *out_angle)
{
    if (s == NULL || out_angle == NULL)
    {
        ESP_LOGE(TAG, "get_angle: parameter is NULL");
        return false;
    }

    if (!s->inited)
    {
        ESP_LOGE(TAG, "get_angle: not inited");
        return false;
    }

    *out_angle = s->angle;
    return true;
}












