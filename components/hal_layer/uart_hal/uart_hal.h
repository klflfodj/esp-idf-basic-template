#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ========== UART Port ========== //
typedef enum
{
    HAL_UART_0 = 0,
    HAL_UART_1 = 1,
    HAL_UART_2 = 2,
    HAL_UART_MAX = 3,
} hal_uart_port_t;  // 串口端口

bool hal_uart_init(hal_uart_port_t port, int tx_pin, int rx_pin, uint32_t baud, uint32_t rx_buf_size);
bool hal_uart_write(hal_uart_port_t port, const void* data, size_t len);
bool hal_uart_read(hal_uart_port_t port, void* buf, size_t max_len, size_t* out_len, uint32_t timeout_ms);
bool hal_uart_flush(hal_uart_port_t port);
bool hal_uart_deinit(hal_uart_port_t port);



