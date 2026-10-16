set(src_dirs
            led_drv
            key_drv)

set(include_dirs
            led_drv
            key_drv)

set(requires
            hal_layer)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
