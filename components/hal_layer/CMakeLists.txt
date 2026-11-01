set(src_dirs
            gpio_hal
            intr_hal
            adc_hal
            uart_hal
            iic_hal
            spi_hal)

set(include_dirs
            gpio_hal
            intr_hal
            adc_hal
            uart_hal
            iic_hal
            spi_hal)

set(requires
            esp_driver_gpio
            esp_adc
            esp_driver_uart
            esp_driver_i2c
            esp_driver_spi)

idf_component_register(
    SRC_DIRS ${src_dirs} 
    INCLUDE_DIRS ${include_dirs} 
    REQUIRES ${requires}
)
