#include "uart_hal.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "soc/soc_caps.h"

// 串口端口数量
#define HAL_UART_PORT_CNT SOC_UART_NUM
// 串口端口是否初始化
static bool s_inited[HAL_UART_PORT_CNT];

// 检查串口端口是否存在
static bool hal_uart_port_check(hal_uart_port_t port)
{
    if ((int)port < 0 || (int)port >= SOC_UART_NUM)
    {
        printf("uart_hal: port %d does not exist\n", (int)port);
        return false;
    }
    return true;
}

// 初始化串口
bool hal_uart_init(const hal_uart_cfg_t* cfg)
{
    if (cfg == NULL)
    {
        printf("uart_hal: cfg is NULL\n");
        return false;
    }

    hal_uart_port_t port = cfg -> port;

    if (!hal_uart_port_check(port))
    {
        return false;
    }

    // 检查是否已初始化
    if (s_inited[port])
    {
        return true;
    }

    // 检查接收缓冲区大小是否有效
    uint32_t rx_buf_size = cfg -> rx_buf_size;
    if (rx_buf_size < 256)
    {
        rx_buf_size = 1024;
    }

    // 配置串口参数（变量名用 ucfg，避免和参数 cfg 重名）
    uart_config_t ucfg = 
    {
        .baud_rate  = (int)cfg -> baud,         // 波特率
        .data_bits  = UART_DATA_8_BITS,         // 数据位
        .parity     = UART_PARITY_DISABLE,      // 校验位
        .stop_bits  = UART_STOP_BITS_1,         // 停止位
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, // 软件流控制
        .source_clk = UART_SCLK_DEFAULT,        // 时钟源
    };

    // 配置串口参数
    if (uart_param_config((uart_port_t)port, &ucfg) != ESP_OK)
    {
        printf("uart_hal: param config failed\n");
        return false;
    }

    // 设置串口引脚
    if (uart_set_pin((uart_port_t)port, cfg -> tx_pin, cfg -> rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
    {
        printf("uart_hal: set pin failed\n");
        return false;
    }

    // 安装串口驱动
    if (uart_driver_install((uart_port_t)port, (int)rx_buf_size, 0, 0, NULL, 0) != ESP_OK)
    {
        printf("uart_hal: driver install failed\n");
        return false;
    }

    s_inited[port] = true;
    return true;
}

// 写入数据到串口
bool hal_uart_write(hal_uart_port_t port, const void* data, size_t len)
{
    if (!hal_uart_port_check(port) || data == NULL || len == 0)
    {
        return false;
    }

    if (!s_inited[port])
    {
        printf("uart_hal: port %d not inited\n", (int)port);
        return false;
    }

    // 写入数据到串口
    int written = uart_write_bytes((uart_port_t)port, data, len);
    return (written == (int)len);
}

// 从串口读取数据
bool hal_uart_read(hal_uart_port_t port, void* buf, size_t max_len, size_t* out_len, uint32_t timeout_ms)
{
    if (!hal_uart_port_check(port) || buf == NULL || out_len == NULL || max_len == 0)
    {
        return false;
    }

    *out_len = 0;  // 初始化读取长度为0

    if (!s_inited[port])
    {
        printf("uart_hal: port %d not inited\n", (int)port);
        return false;
    }

    // 从串口读取数据
    int got = uart_read_bytes((uart_port_t)port, buf, (uint32_t)max_len, pdMS_TO_TICKS(timeout_ms));

    if (got < 0)
    {
        return false;
    }

    // 更新读取长度
    *out_len = (size_t)got;
    return true;
}

// 清空串口缓冲区
bool hal_uart_flush(hal_uart_port_t port)
{
    if (!hal_uart_port_check(port) || !s_inited[port])
    {
        return false;
    }

    return (uart_flush_input((uart_port_t)port) == ESP_OK);
}

// 反初始化串口
bool hal_uart_deinit(hal_uart_port_t port)
{
    if (!hal_uart_port_check(port))
    {
        return false;
    }

    if (!s_inited[port])
    {
        return true;
    }

    // 移除串口驱动
    if (uart_driver_delete((uart_port_t)port) != ESP_OK)
    {
        return false;
    }

    s_inited[port] = false;
    return true;
}

