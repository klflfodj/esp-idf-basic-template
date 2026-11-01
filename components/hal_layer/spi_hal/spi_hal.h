#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** SPI HAL：填结构体 → 调一次函数，完成一次传输
 *
 *  ==================== 怎么用 ====================
 *
 *    // ① 建总线（引脚）
 *    hal_spi_cfg_t bus = {
 *        .host     = HAL_SPI_HOST_2,   // SPI2（HSPI）
 *        .mosi_pin = 23,
 *        .miso_pin = 19,
 *        .sclk_pin = 18,
 *    };
 *    hal_spi_init(&bus);
 *
 *    // ② 注册设备（每片从设备一次：CS + 频率 + 模式）
 *    hal_spi_dev_cfg_t dev = {
 *        .host    = HAL_SPI_HOST_2,
 *        .cs_pin  = 5,
 *        .freq_hz = 1000000,
 *        .mode    = 0,
 *    };
 *    hal_spi_dev_add(&dev);
 *
 *    // ③ 发 1 字节命令，读 4 字节数据（传感器最常用）
 *    uint8_t id[4];
 *    hal_spi_write_read(HAL_SPI_HOST_2, 5, cmd, id, 4);
 *
 *  ==================== 必须知道的几条 ====================
 *
 *   1. 【SPI1 不能用】它接 flash，一动就崩。本驱动只开放 SPI2 / SPI3。
 *
 *   2. 【引脚不能用 6~11】（flash）和 34~39（只能输入）。
 *      常用组合：MOSI=23, MISO=19, SCLK=18, CS=5。
 *
 *   3. 【mode 0~3】由设备的 CPOL/CPHA 决定，查手册。不确定就依次试 0/1/2/3。
 *
 *   4. 【SPI 是全双工】收数据的同时必然在发数据。所以"发 N 字节读 N 字节"
 *      用同一个函数一次完成；"发 1 字节读 N 字节"时，后面自动补 0xFF
 *      （绝大多数器件的 dummy 字节就是 0xFF）。
 *
 *   5. 【CS 由驱动控制】传了 cs_pin 后，每次传输前后驱动自动拉低/拉高。
 *      如果你要手动控制 CS（比如一次事务分几次调用），cs_pin 传 -1。
 *
 *   6. 频率别一上来就拉高。先从 1MHz 试，读得到再往上加；
 *      跑不稳先降到 1MHz 甚至 100kHz。
 */

typedef enum 
{
    HAL_SPI_HOST_2 = 0,  
    HAL_SPI_HOST_3 = 1,
    HAL_SPI_HOST_MAX,
} hal_spi_host_t;

typedef struct
{
    hal_spi_host_t host;            // SPI 总线号
    int            mosi_pin;        // MOSI 引脚号
    int            miso_pin;        // MISO 引脚号
    int            sclk_pin;        // SCLK 引脚号
    int            max_transfer_sz; // 最大传输大小（字节）
} hal_spi_cfg_t;

typedef struct
{
    hal_spi_host_t host;      // SPI 总线号
    int            cs_pin;    // CS 引脚号
    uint32_t       freq_hz;   // SCLK 频率，先试 1000000
    uint8_t        mode;      // SPI 模式 0~3（CPOL/CPHA），查手册
} hal_spi_dev_cfg_t;

typedef struct
{
    uint8_t *tx_buf; // 发送缓冲区
    size_t   tx_len; // 发送长度（字节）
    uint8_t *rx_buf; // 接收缓冲区
    size_t   rx_len; // 接收长度（字节）
} hal_spi_msg_t;

typedef struct
{
    int                 cs_pin; // CS 引脚号
    spi_device_handle_t handle; // SPI 设备句柄
} spi_dev_slot_t;

bool hal_spi_init(const hal_spi_cfg_t *cfg);
bool hal_spi_dev_add(const hal_spi_dev_cfg_t *cfg);
bool hal_spi_transfer(hal_spi_host_t host, int cs_pin, const hal_spi_msg_t *msg);
bool hal_spi_write(hal_spi_host_t host, int cs_pin, const void *data, size_t len);
bool hal_spi_read(hal_spi_host_t host, int cs_pin, void *buf, size_t len);
bool hal_spi_write_read(hal_spi_host_t host, int cs_pin, const void *tx, void *rx, size_t len);
