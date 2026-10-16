set(src_dirs
            gpio_hal)

set(include_dirs
            gpio_hal)

set(requires
            esp_driver_gpio)

idf_component_register(
    SRC_DIRS ${src_dirs} 
    INCLUDE_DIRS ${include_dirs} 
    REQUIRES ${requires}
)
