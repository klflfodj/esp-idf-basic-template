set(src_dirs
            led_drv
            key_drv
            oled_drv
            pot_drv
            servo_drv)

set(include_dirs
            led_drv
            key_drv
            oled_drv
            pot_drv
            servo_drv)

set(requires
            hal_layer
            nixy4__u8g2)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
