#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** IIC HAL：填一个结构体，调一次函数，完成一次传输
 *
 *  ==================== 怎么用 ====================
 *
 *    // ① 建总线（整个程序一次就够）
 *    hal_iic_cfg_t cfg = {
 *        .bus     = HAL_IIC_BUS_0,
 *        .scl_pin = 22,          // 时钟
 *        .sda_pin = 21,          // 数据
 *        .freq_hz = 100000,      // 100kHz（标准模式）
 *    };
 *    hal_iic_init(&cfg);
 *
 *    // ② 读一个寄存器（最常用）
 *    uint8_t id = 0;
 *    if (hal_iic_read_reg(HAL_IIC_BUS_0, 0x68, 0x75, &id, 1))
 *    {
 *        printf("who_am_i = 0x%02X\n", id);
 *    }
 *
 *    // ③ 写一个寄存器
 *    hal_iic_write_reg(HAL_IIC_BUS_0, 0x68, 0x6B, 0x00);   // 唤醒 MPU6050
 *
 *    // ④ 连读多个字节（比如加速度 6 个字节）
 *    uint8_t acc[6];
 *    hal_iic_read_reg(HAL_IIC_BUS_0, 0x68, 0x3B, acc, 6);
 *
 *    // ⑤ 自由组合：用结构体描述"这次传输要干什么"
 *    uint8_t reg = 0x3B;
 *    uint8_t buf[6];
 *    hal_iic_msg_t msg = {
 *        .addr   = 0x68,
 *        .tx_buf = &reg,      // 先发：寄存器地址
 *        .tx_len = 1,
 *        .rx_buf = buf,       // 再收：6 个字节
 *        .rx_len = 6,
 *    };
 *    hal_iic_transfer(HAL_IIC_BUS_0, &msg);
 *
 *  ==================== 必须知道的几条 ====================
 *
 *   1. 【必须外接上拉电阻】SCL 和 SDA 各接一个 4.7kΩ 到 3.3V。
 *      IIC 是开漏输出，只能拉低不能拉高，没上拉就永远是低电平，什么都读不到。
 *      ESP32 内部上拉只有约 45kΩ，高速下不够用，所以本驱动默认不开内部上拉。
 *
 *   2. 【引脚不能用 6~11】（接 flash）和 34~39（只能输入）。
 *      常用组合：SDA=GPIO21，SCL=GPIO22。
 *
 *   3. 【地址是 7 位】。手册常给 8 位地址（含读写位），要右移一位：
 *      0xD0 → 0x68。传超过 0x7F 的地址肯定是忘了移位。
 *
 *   4. IIC 没有"波特率"，只有 SCL 频率：
 *        100000  = 100kHz，标准模式，最稳
 *        400000  = 400kHz，快速模式，线短、上拉够强才能跑稳
 *      跑不稳就先降到 100k，绝大多数问题都会消失。
 *
 *   5. 一条总线上的设备共享 SCL/SDA，靠【地址】区分，地址不能重复。
 *
 *   6. 每次传输超时固定 1 秒。总线被拉死时函数会返回 false，不会永远卡住。
 */

// IIC 总线
typedef enum
{
    HAL_IIC_BUS_0 = 0,  // IIC0
    HAL_IIC_BUS_1 = 1,  // IIC1
    HAL_IIC_BUS_MAX,    // 最大总线数
} hal_iic_bus_t;  // IIC 总线

// IIC 配置
typedef struct
{
    hal_iic_bus_t bus;      // 用哪条总线
    int           scl_pin;  // SCL 时钟脚，例如 22
    int           sda_pin;  // SDA 数据脚，例如 21
    uint32_t      freq_hz;  // SCL 频率(Hz)：100000 或 400000
} hal_iic_cfg_t;  // IIC 配置

// IIC 消息
typedef struct
{
    uint8_t        addr;    // 目标设备 7 位地址，例如 0x68
    const uint8_t *tx_buf;  // 发送缓冲区
    size_t         tx_len;  // 发送长度
    uint8_t       *rx_buf;  // 接收缓冲区
    size_t         rx_len;  // 接收长度
} hal_iic_msg_t;  // IIC 消息


bool hal_iic_init(const hal_iic_cfg_t *cfg);
bool hal_iic_transfer(hal_iic_bus_t bus, const hal_iic_msg_t *msg);
bool hal_iic_write(hal_iic_bus_t bus, uint8_t addr, const void *data, size_t len);
bool hal_iic_read(hal_iic_bus_t bus, uint8_t addr, void *buf, size_t len);
bool hal_iic_read_reg(hal_iic_bus_t bus, uint8_t addr, uint8_t reg,
                      void *buf, size_t len);
bool hal_iic_write_reg(hal_iic_bus_t bus, uint8_t addr, uint8_t reg, uint8_t value);
bool hal_iic_probe(hal_iic_bus_t bus, uint8_t addr);
