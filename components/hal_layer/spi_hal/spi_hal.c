#include "spi_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "soc/soc_caps.h"

// 一次传输的最大字节数
#define HAL_SPI_XFER_MAX  256
// 每条总线上的最大设备数
#define HAL_SPI_DEV_MAX   8
// 总线是否初始化过
static spi_device_handle_t s_bus_ready[HAL_SPI_HOST_MAX]; 
// 设备槽位：CS 引脚 + 句柄。这是内部实现，所以只放在 .c 里
static spi_dev_slot_t s_dev[HAL_SPI_HOST_MAX][HAL_SPI_DEV_MAX]; // 设备槽位
static uint8_t        s_dev_cnt[HAL_SPI_HOST_MAX];  // 设备槽位计数
// 补齐用的静态缓冲区
static uint8_t s_dummy[HAL_SPI_XFER_MAX];

// 转换 spi 总线号为 spi_host_device_t 类型
static bool hal_spi_host_resolve(hal_spi_host_t host, spi_host_device_t* out)
{
    switch (host)
    {
        case HAL_SPI_HOST_2: *out = SPI2_HOST; return true;
        case HAL_SPI_HOST_3: *out = SPI3_HOST; return true;
        default:
            printf("spi_hal: host %d not supported\n", (int)host);
            return false;
    }
}

// 按照 cs 引脚号获取设备句柄
static spi_device_handle_t hal_spi_dev_get(hal_spi_host_t host, int cs_pin)
{
    for (uint8_t i = 0; i < s_dev_cnt[host]; i++)
    {
        if (s_dev[host][i].cs_pin == cs_pin)
        {
            return s_dev[host][i].handle;
        }
    }
    printf("spi_hal: cs %d not registered, call hal_spi_dev_add() first\n", cs_pin);
    return NULL;
}

// 初始化一条 SPI 总线
bool hal_spi_init(const hal_spi_cfg_t* cfg)
{
    if (cfg == NULL)
    {
        printf("spi_hal: cfg is NULL\n");
        return false;
    }

    spi_host_device_t idf_host;

    // 检查总线号是否合法
    if (!hal_spi_host_resolve(cfg->host, &idf_host))
    {
        return false;
    }
    // 幂等：已经建过就直接成功
    if (s_bus_ready[cfg -> host] != NULL)
    {
        return true;
    }

    // 配置总线
    spi_bus_config_t bus_cfg = 
    {
        .mosi_io_num     = cfg->mosi_pin,          // MOSI 引脚号
        .miso_io_num     = cfg->miso_pin,          // MISO 引脚号
        .sclk_io_num     = cfg->sclk_pin,          // SCLK 引脚号  
        .quadwp_io_num   = -1,                     // WP 引脚号，没用就 -1
        .quadhd_io_num   = -1,                     // HD 引脚号，没用就 -1
        .max_transfer_sz = cfg->max_transfer_sz,   // 0 = 驱动默认（4092）
    };

    // 初始化总线
    if (spi_bus_initialize(idf_host, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK)
    {
        printf("spi_hal: bus init failed\n");
        return false;
    }

    s_bus_ready[cfg -> host] = (spi_device_handle_t)1; // 标记总线已初始化
    return true;
}

// 添加 SPI 设备到总线
bool hal_spi_dev_add(const hal_spi_dev_cfg_t* cfg)
{
    if (cfg == NULL)
    {
        printf("spi_hal: dev cfg is NULL\n");
        return false;
    }

    spi_host_device_t idf_host;

    // 检查总线号是否合法
    if (!hal_spi_host_resolve(cfg->host, &idf_host))
    {
        return false;
    }

    // 检查总线是否已初始化
    if (s_bus_ready[cfg->host] == NULL)
    {
        printf("spi_hal: call hal_spi_init() first\n");
        return false;
    }

    // 检查模式是否合法
    if (cfg->mode > 3)
    {
        printf("spi_hal: mode must be 0~3\n");
        return false;
    }

    // 检查频率是否合法
    if (cfg -> freq_hz == 0 || cfg -> freq_hz > 80000000)
    {
        printf("spi_hal: freq %u out of range\n", (unsigned)cfg->freq_hz);
        return false;
    }

    // 检查设备槽位是否已存在
    for (uint8_t i = 0; i < s_dev_cnt[cfg -> host]; i++)
    {
        if (s_dev[cfg -> host][i].cs_pin == cfg -> cs_pin)
        {
            return true;
        }
    }

    // 检查设备槽位是否已满
    if (s_dev_cnt[cfg -> host] >= HAL_SPI_DEV_MAX)
    {
        printf("spi_hal: too many devices (max %d)\n", HAL_SPI_DEV_MAX);
        return false;
    }

    spi_device_interface_config_t dev_cfg = 
    {
        .mode           = cfg->mode,            // SPI 模式
        .clock_speed_hz = (int)cfg->freq_hz,    // SPI 时钟频率
        .spics_io_num   = cfg->cs_pin,          // CS 引脚号
        .queue_size     = 1,                    // 传输队列大小
    };

    spi_device_handle_t handle = NULL;

    // 添加设备到总线
    if (spi_bus_add_device(idf_host, &dev_cfg, &handle) != ESP_OK)
    {
        printf("spi_hal: add device failed\n");
        return false;
    }

    // 记录设备槽位
    s_dev[cfg->host][s_dev_cnt[cfg->host]].cs_pin = cfg->cs_pin;  // 记录 CS 引脚号
    s_dev[cfg->host][s_dev_cnt[cfg->host]].handle = handle;       // 记录设备句柄
    s_dev_cnt[cfg->host]++; // 记录设备槽位计数
    return true;
}

// SPI 传输
bool hal_spi_transfer(hal_spi_host_t host, int cs_pin, const hal_spi_msg_t *msg)
{
    if (msg == NULL)
    {
        printf("spi_hal: msg is NULL\n");
        return false;
    }

    // 检查传输数据是否为空
    if ((msg->tx_len > 0 && msg->tx_buf == NULL) ||
        (msg->rx_len > 0 && msg->rx_buf == NULL))
    {
        printf("spi_hal: len > 0 but buffer is NULL\n");
        return false;
    }

    // 检查传输数据是否为空
    if (msg->tx_len == 0 && msg->rx_len == 0)
    {
        printf("spi_hal: nothing to do\n");
        return false;
    }

    // 检查设备槽位是否已存在
    spi_device_handle_t dev = hal_spi_dev_get(host, cs_pin);
    if (dev == NULL)
    {
        return false;
    }

    // 全双工：实际时钟数取两边较长的那个
    size_t len = (msg->tx_len > msg->rx_len) ? msg->tx_len : msg->rx_len;

    // 检查传输数据是否超过最大传输长度
    if (len > HAL_SPI_XFER_MAX)
    {
        printf("spi_hal: transfer %u bytes > max %d\n", (unsigned)len, HAL_SPI_XFER_MAX);
        return false;
    }

    // 发送缓冲区指针
    const uint8_t *tx_ptr = msg->tx_buf;

    /* tx 比 rx 短 → 自己拼一个：前面是用户数据，后面补 0xFF
       （SPI 是全双工，收 N 字节就必须发 N 字节，不能不发） */
    // 检查发送数据是否足够长
    if (msg->tx_len < len)
    {
        // 补 0xFF
        memset(s_dummy, 0xFF, len);
        if (msg->tx_len > 0)
        {
            // 复制用户数据
            memcpy(s_dummy, msg->tx_buf, msg->tx_len);
        }
        // 指向临时缓冲区
        tx_ptr = s_dummy;
    }
    
    // 配置传输参数
    spi_transaction_t trans =
    {
        .length    = len * 8,                                    // 发送数据长度(bit)
        .rxlength  = msg->rx_len * 8,                            // 接收数据长度(bit)
        .tx_buffer = (msg->rx_len == 0) ? msg->tx_buf : tx_ptr,  // 发送缓冲区指针
        .rx_buffer = msg->rx_buf,                                // 接收缓冲区指针
    };

    // 只写时不需要收，rx_buffer 保持 NULL
    if (msg->rx_len == 0)
    {
        trans.rxlength = 0;
    }

    // 执行传输
    if (spi_device_polling_transmit(dev, &trans) != ESP_OK)
    {
        printf("spi_hal: transfer failed\n");
        return false;
    }

    return true;
}

// 只写
bool hal_spi_write(hal_spi_host_t host, int cs_pin, const void *data, size_t len)
{
    hal_spi_msg_t msg = { .tx_buf = (const uint8_t *)data, .tx_len = len };  // 创建传输消息结构体
    return hal_spi_transfer(host, cs_pin, &msg);
}

// 只读
bool hal_spi_read(hal_spi_host_t host, int cs_pin, void *buf, size_t len)
{
    hal_spi_msg_t msg = { .rx_buf = (uint8_t *)buf, .rx_len = len };
    return hal_spi_transfer(host, cs_pin, &msg);
}

// 写读
bool hal_spi_write_read(hal_spi_host_t host, int cs_pin, const void *tx, void *rx, size_t len)
{
    hal_spi_msg_t msg =
    {
        .tx_buf = (const uint8_t *)tx,
        .tx_len = len,
        .rx_buf = (uint8_t *)rx,
        .rx_len = len,
    };
    return hal_spi_transfer(host, cs_pin, &msg);
}
