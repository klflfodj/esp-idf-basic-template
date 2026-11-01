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

// ========== 总线号 ========== //
/** 总线编号，对应芯片的 IIC0 / IIC1 */
typedef enum
{
    HAL_IIC_BUS_0 = 0,   /**< IIC0 */
    HAL_IIC_BUS_1 = 1,   /**< IIC1 */
    HAL_IIC_BUS_MAX,     /**< 内部用：枚举上界 */
} hal_iic_bus_t;

// ========== 总线配置 ========== //
/** 初始化总线时要填的东西 */
typedef struct
{
    hal_iic_bus_t bus;      /**< 用哪条总线 */
    int           scl_pin;  /**< SCL 时钟脚，例如 22 */
    int           sda_pin;  /**< SDA 数据脚，例如 21 */
    uint32_t      freq_hz;  /**< SCL 频率(Hz)：100000 或 400000 */
} hal_iic_cfg_t;

// ========== 一次传输 ========== //
/** 描述"这一次传输要干什么"
 *
 *    tx 有、rx 无   → 只写
 *    tx 无、rx 有   → 只读
 *    tx 有、rx 有   → 先写后读（读寄存器就是这种，中间不释放总线）
 */
typedef struct
{
    uint8_t        addr;    /**< 目标设备 7 位地址，例如 0x68 */
    const uint8_t *tx_buf;  /**< 要发的字节；只读时传 NULL */
    size_t         tx_len;  /**< 发多少字节；只读时填 0 */
    uint8_t       *rx_buf;  /**< 收数据的缓冲区；只写时传 NULL */
    size_t         rx_len;  /**< 想收多少字节；只写时填 0 */
} hal_iic_msg_t;


/** 初始化一条 IIC 总线
 *
 *  @param  cfg  总线配置（总线号、两个引脚、频率）
 *  @return true 成功；false = 配置为空 / 总线号非法 / 频率越界 / 建总线失败
 *
 *  重复初始化同一条总线是安全的（幂等），不会重复申请资源。
 *  @note 没接外部上拉电阻时本函数仍然返回 true，但后面 probe/读写会全部失败。
 */
bool hal_iic_init(const hal_iic_cfg_t *cfg);


/** 执行一次传输（所有操作的底层入口）
 *
 *  @param  bus  总线号
 *  @param  msg  这次要干什么（见 hal_iic_msg_t 的说明）
 *  @return true 成功（设备回了 ACK）；false = 参数错误 / 总线没初始化 /
 *                地址不是 7 位 / 设备不应答 / 总线出错 / 超时
 *
 *  tx 和 rx 都为空是无效调用，直接返回 false。
 */
bool hal_iic_transfer(hal_iic_bus_t bus, const hal_iic_msg_t *msg);


/** 只写
 *  @return true 成功；false 含义同 hal_iic_transfer
 */
bool hal_iic_write(hal_iic_bus_t bus, uint8_t addr, const void *data, size_t len);


/** 只读（不先写寄存器地址）
 *  @return true 成功；false 含义同 hal_iic_transfer
 *
 *  适用于"一上来就吐数据"的器件。绝大多数器件要先写寄存器地址，
 *  那种情况用 hal_iic_read_reg()。
 */
bool hal_iic_read(hal_iic_bus_t bus, uint8_t addr, void *buf, size_t len);


/** 读寄存器（先写寄存器地址，再连续读）—— 最常用的操作
 *
 *  @param  bus   总线号
 *  @param  addr  7 位地址
 *  @param  reg   寄存器地址（8 位）
 *  @param  buf   接收缓冲区
 *  @param  len   读几个字节（读一个传 1，连读传 N）
 *  @return true 成功；false 含义同 hal_iic_transfer
 *
 *  注意：寄存器地址如果是 16 位的（少数器件），这个函数不适用，
 *        请用 hal_iic_transfer() 自己填 tx_len = 2。
 */
bool hal_iic_read_reg(hal_iic_bus_t bus, uint8_t addr, uint8_t reg,
                      void *buf, size_t len);


/** 写寄存器（发"寄存器地址 + 一个字节"）
 *  @return true 成功；false 含义同 hal_iic_transfer
 */
bool hal_iic_write_reg(hal_iic_bus_t bus, uint8_t addr, uint8_t reg, uint8_t value);


/** 探测某个地址上有没有设备
 *
 *  @param  bus   总线号
 *  @param  addr  7 位地址
 *  @return true 有设备应答；false 没人应答 / 总线没初始化
 *
 *  【调试第一步就调它】。建议扫一遍 0x08 ~ 0x77。
 */
bool hal_iic_probe(hal_iic_bus_t bus, uint8_t addr);
