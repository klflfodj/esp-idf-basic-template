#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** UART HAL：串口收发（8 数据位 / 无校验 / 1 停止位 / 无流控）
 *
 *  ==================== 怎么用 ====================
 *
 *    // ① 初始化（配置放在结构体里，每个值都有名字，不会把 TX/RX 传反）
 *    hal_uart_cfg_t cfg =
 *    {
 *        .port        = HAL_UART_1,   // 端口 1（别用 0）
 *        .tx_pin      = 17,           // 我的输出
 *        .rx_pin      = 16,           // 我的输入
 *        .baud        = 115200,
 *        .rx_buf_size = 1024,
 *    };
 *    hal_uart_init(&cfg);
 *
 *    // ② 发
 *    hal_uart_write(HAL_UART_1, "cmd\n", 4);
 *
 *    // ③ 收（最多等 100ms）
 *    char buf[64];
 *    size_t got = 0;
 *    if (hal_uart_read(HAL_UART_1, buf, sizeof(buf), &got, 100) && got > 0)
 *    {
 *        printf("收到 %u 字节\n", (unsigned)got);
 *    }
 *
 *  ==================== 必须知道的几条 ====================
 *
 *    1. 【别用 UART0】它是 log / printf 的输出口。占了它，你的收发会和调试输出搅在一起。
 *       用自己的口就选端口 1 或 2。
 *
 *    2. 引脚是随便配的（ESP32 的 UART 能路由到任意 GPIO），
 *       但 【6~11 绝对不能用】（接 flash 的，一动就崩）。
 *
 *    3. 两个设备互连要【交叉接线】：我的 TX 接你的 RX，我的 RX 接你的 TX，
 *       GND 必须共地。TX 接 TX 是收不到任何东西的（常见错误）。
 *
 *    4. 上电后第一次读之前先 hal_uart_flush()。引脚上电瞬间的电平跳变
 *       会被当成数据，第一包经常是乱码。
 *
 *    5. 读超时不算失败：
 *         return false       → 真出错（端口非法 / 没初始化 / 参数为空）
 *         return true, got=0 → 正常，但这段时间没数据
 *
 *    6. 写是【阻塞】的（发送缓冲为 0），几十字节随便用；
 *       要发大块数据请放到任务里，别在中断里发。
 */

// ========== UART 端口 ========== //
/** 串口号，对应 ESP32 的 UART0 / UART1 / UART2 */
typedef enum
{
    HAL_UART_0 = 0,   /**< UART0。默认给 log / printf 用，一般别碰 */
    HAL_UART_1 = 1,   /**< UART1，推荐 */
    HAL_UART_2 = 2,   /**< UART2，推荐 */
    HAL_UART_MAX,     /**< 内部用：枚举上界 */
} hal_uart_port_t;

// ========== 串口配置 ========== //
/** 初始化时要填的东西 */
typedef struct
{
    hal_uart_port_t port;         /**< 用哪个串口 */
    int             tx_pin;       /**< 发送引脚（我的输出） */
    int             rx_pin;       /**< 接收引脚（我的输入） */
    uint32_t        baud;         /**< 波特率，常用 115200 / 9600 */
    uint32_t        rx_buf_size;  /**< 驱动内部接收缓冲(字节)，建议 >= 1024 */
} hal_uart_cfg_t;


/** 初始化串口
 *
 *  @param  cfg  串口配置（端口、两个引脚、波特率、接收缓冲大小）
 *  @return true 成功；false = 配置为空 / 端口非法 / 参数配置失败 /
 *                         引脚设置失败 / 装驱动失败
 *
 *  固定 8N1（8 数据位、无校验、1 停止位、无硬件流控）。
 *  重复调用同一个端口是安全的（幂等），不会重复申请资源。
 *
 *  要注意的后果：
 *  - rx_buf_size 给太小（比如 128），高速收到长包时后面的字节会被丢掉。
 *  - 引脚和已有的 GPIO 用途冲突时，那个 GPIO 的功能会被抢走。
 */
bool hal_uart_init(const hal_uart_cfg_t *cfg);


/** 发送数据
 *
 *  @param  port  端口号
 *  @param  data  数据起始地址
 *  @param  len   字节数
 *  @return true 全部发送成功；false = 参数为空 / 端口没初始化 / 底层写入失败
 *
 *  阻塞式：等到数据全部塞进硬件才返回，所以返回后就可以复用 data 缓冲区。
 */
bool hal_uart_write(hal_uart_port_t port, const void *data, size_t len);


/** 接收数据
 *
 *  @param  port        端口号
 *  @param  buf         接收缓冲区
 *  @param  max_len     buf 最多能放多少字节
 *  @param  out_len     输出：实际收到多少字节。【只在返回 true 时有效】
 *  @param  timeout_ms  最多等多久(ms)。传 0 = 立刻返回
 *  @return true 执行正常（out_len 可能是 0，表示没数据）；
 *          false = 参数为空 / 端口没初始化 / 底层读失败
 */
bool hal_uart_read(hal_uart_port_t port, void *buf, size_t max_len,
                   size_t *out_len, uint32_t timeout_ms);


/** 清空接收缓冲
 *
 *  @param  port  端口号
 *  @return true 成功
 *
 *  上电初始化之后、重新连接上位机之后、解析出错要重新同步时调用。
 */
bool hal_uart_flush(hal_uart_port_t port);


/** 关闭串口并释放驱动
 *
 *  @param  port  端口号
 *  @return true 成功（没初始化过也算成功）
 */
bool hal_uart_deinit(hal_uart_port_t port);
