#include "iic_hal.h"

#include <stdio.h>
#include "driver/i2c_master.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

// IIC 传输超时时间，单位：毫秒
#define HAL_IIC_XFER_TIMEOUT_MS   1000
// 每条总线上的最大设备地址
#define HAL_IIC_DEV_MAX           8


static const char* TAG = "iic_hal";


// IIC 总线句柄
static i2c_master_bus_handle_t s_bus[SOC_I2C_NUM] = {0};
// IIC 总线频率，单位：赫兹
static uint32_t s_bus_freq[SOC_I2C_NUM];

// IIC 设备槽位
typedef struct
{
    uint8_t                 addr;    // 设备地址
    i2c_master_dev_handle_t handle;  // 设备句柄
} iic_dev_slot_t;  // IIC 设备槽位

static iic_dev_slot_t s_dev[SOC_I2C_NUM][HAL_IIC_DEV_MAX];
// 每条总线上的设备数量
static uint8_t s_dev_cnt[SOC_I2C_NUM];


// 检查总线是否存在
static bool hal_iic_bus_check(hal_iic_bus_t bus)
{
    if ((int)bus < 0 || (int)bus >= SOC_I2C_NUM)
    {
        ESP_LOGE(TAG, "bus %d does not exist", (int)bus);
        return false;
    }
    return true;
}


// 获取设备句柄
static i2c_master_dev_handle_t hal_iic_dev_get(hal_iic_bus_t bus, uint8_t addr)
{
    // 检查总线是否存在
    for (uint8_t i = 0; i < s_dev_cnt[bus]; i++)
    {
        if (s_dev[bus][i].addr == addr)
        {
            return s_dev[bus][i].handle;
        }
    }

    // 检查总线是否已满
    if (s_dev_cnt[bus] >= HAL_IIC_DEV_MAX)
    {
        ESP_LOGE(TAG, "too many devices on bus %d (max %d)",
               (int)bus, HAL_IIC_DEV_MAX);
        return NULL;
    }

    // 配置设备
    i2c_device_config_t dev_cfg =
    {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,   // 7 位地址
        .device_address  = addr,                 // 设备地址
        .scl_speed_hz    = s_bus_freq[bus],      // 跟总线同频率
        .scl_wait_us     = 0,                    // 0 = 用芯片默认值
    };

    // 申请设备句柄
    i2c_master_dev_handle_t handle = NULL;
    // 加入总线
    if (i2c_master_bus_add_device(s_bus[bus], &dev_cfg, &handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "add device 0x%02X failed", addr);
        return NULL;
    }

    // 缓存设备句柄
    s_dev[bus][s_dev_cnt[bus]].addr   = addr;
    s_dev[bus][s_dev_cnt[bus]].handle = handle;
    s_dev_cnt[bus]++;
    return handle;
}


// 传输前的公共检查
static i2c_master_dev_handle_t hal_iic_dev_acquire(hal_iic_bus_t bus, uint8_t addr)
{
    if (!hal_iic_bus_check(bus))
    {
        return NULL;
    }

    if (s_bus[bus] == NULL)
    {
        ESP_LOGE(TAG, "bus %d not inited", (int)bus);
        return NULL;
    }

    // 检查地址是否为 7 位
    if (addr > 0x7F)
    {
        ESP_LOGE(TAG, "addr 0x%02X is not a 7-bit address (try >> 1)", addr);
        return NULL;
    }

    return hal_iic_dev_get(bus, addr);
}


// 初始化一条 IIC 总线
bool hal_iic_init(const hal_iic_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        ESP_LOGE(TAG, "cfg is NULL");
        return false;
    }

    hal_iic_bus_t bus = cfg->bus;

    if (!hal_iic_bus_check(bus))
    {
        return false;
    }

    // 幂等：已经建过就直接成功
    if (s_bus[bus] != NULL)
    {
        return true;
    }

    if (cfg->freq_hz < 1000 || cfg->freq_hz > 1000000)
    {
        ESP_LOGE(TAG, "freq %u out of range (1000 ~ 1000000)",
               (unsigned)cfg->freq_hz);
        return false;
    }

    // 配置总线
    i2c_master_bus_config_t bus_cfg =
    {
        .i2c_port          = (i2c_port_num_t)bus,       // 总线编号
        .sda_io_num        = (gpio_num_t)cfg->sda_pin,  // SDA 数据脚
        .scl_io_num        = (gpio_num_t)cfg->scl_pin,  // SCL 时钟脚
        .clk_source        = I2C_CLK_SRC_DEFAULT,       // 时钟源
        .glitch_ignore_cnt = 7,                         // 滤掉 7 个时钟周期内的毛刺
        .flags.enable_internal_pullup = false,          // 内部上拉太弱，必须外部上拉
    };

    if (i2c_new_master_bus(&bus_cfg, &s_bus[bus]) != ESP_OK)
    {
        ESP_LOGE(TAG, "new master bus failed");
        return false;
    }

    s_bus_freq[bus] = cfg->freq_hz;     // 缓存频率，加设备时要用
    return true;
}


// 一次传输
bool hal_iic_transfer(hal_iic_bus_t bus, const hal_iic_msg_t *msg)
{
    if (msg == NULL)
    {
        ESP_LOGE(TAG, "msg is NULL");
        return false;
    }

    // 填了长度就必须给缓冲区
    if ((msg->tx_len > 0 && msg->tx_buf == NULL) ||
        (msg->rx_len > 0 && msg->rx_buf == NULL))
    {
        ESP_LOGE(TAG, "tx_buf or rx_buf is NULL");
        return false;
    }

    if (msg->tx_len == 0 && msg->rx_len == 0)
    {
        ESP_LOGE(TAG, "tx_len and rx_len are both 0");
        return false;
    }

    i2c_master_dev_handle_t dev = hal_iic_dev_acquire(bus, msg->addr);
    if (dev == NULL)
    {
        return false;
    }

    esp_err_t err;

    if (msg->tx_len > 0 && msg->rx_len > 0)
    {
        // 先写后读（读寄存器），中间不释放总线
        err = i2c_master_transmit_receive(dev,
                                          msg->tx_buf, msg->tx_len,
                                          msg->rx_buf, msg->rx_len,
                                          HAL_IIC_XFER_TIMEOUT_MS);
    }
    else if (msg->tx_len > 0)
    {
        // 只写
        err = i2c_master_transmit(dev, msg->tx_buf, msg->tx_len,
                                  HAL_IIC_XFER_TIMEOUT_MS);
    }
    else
    {
        // 只读
        err = i2c_master_receive(dev, msg->rx_buf, msg->rx_len,
                                 HAL_IIC_XFER_TIMEOUT_MS);
    }

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "transfer to 0x%02X failed: %s",
               msg->addr, esp_err_to_name(err));
        return false;
    }

    return true;
}


// 只写
bool hal_iic_write(hal_iic_bus_t bus, uint8_t addr, const void *data, size_t len)
{
    hal_iic_msg_t msg =
    {
        .addr   = addr,
        .tx_buf = (const uint8_t *)data,
        .tx_len = len,
    };
    return hal_iic_transfer(bus, &msg);
}


// 只读
bool hal_iic_read(hal_iic_bus_t bus, uint8_t addr, void *buf, size_t len)
{
    hal_iic_msg_t msg =
    {
        .addr   = addr,
        .rx_buf = (uint8_t *)buf,
        .rx_len = len,
    };
    return hal_iic_transfer(bus, &msg);
}


// 读寄存器：先发 1 字节寄存器地址，再读 len 字节
bool hal_iic_read_reg(hal_iic_bus_t bus, uint8_t addr, uint8_t reg,
                      void *buf, size_t len)
{
    hal_iic_msg_t msg =
    {
        .addr   = addr,
        .tx_buf = &reg,          // 局部变量的地址，本次调用内有效
        .tx_len = 1,
        .rx_buf = (uint8_t *)buf,
        .rx_len = len,
    };
    return hal_iic_transfer(bus, &msg);
}


// 写寄存器：发"寄存器地址 + 一个字节"
bool hal_iic_write_reg(hal_iic_bus_t bus, uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t cmd[2] = { reg, value };

    hal_iic_msg_t msg =
    {
        .addr   = addr,
        .tx_buf = cmd,
        .tx_len = 2,
    };
    return hal_iic_transfer(bus, &msg);
}


// 探测设备是否存在
bool hal_iic_probe(hal_iic_bus_t bus, uint8_t addr)
{
    if (!hal_iic_bus_check(bus))
    {
        return false;
    }

    if (s_bus[bus] == NULL)
    {
        ESP_LOGE(TAG, "bus %d not inited", (int)bus);
        return false;
    }

    return (i2c_master_probe(s_bus[bus], addr, HAL_IIC_XFER_TIMEOUT_MS) == ESP_OK);
}
