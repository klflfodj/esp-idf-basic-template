#pragma once

#include <stdint.h>
#include <stdbool.h>

// ========== GPIO ISR ========== //
typedef void (*hal_gpio_isr_cb_t)(void *arg);


bool hal_gpio_isr_init(void);
bool hal_gpio_isr_register(int gpio_num, hal_gpio_isr_cb_t cb, void *arg);

