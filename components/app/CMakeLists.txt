set(src_dirs
            ".")

set(include_dirs
            ".")

set(requires
            svc
            bsp
            drv
            hal_layer)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
