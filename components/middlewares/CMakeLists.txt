set(src_dirs
            eye_expression)

set(include_dirs
            eye_expression)

set(requires
            drv
            esp_timer
            esp_hw_support)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
