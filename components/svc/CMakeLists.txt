set(src_dirs
            filter_lowpass_svc
            wifi_svc)

set(include_dirs
            filter_lowpass_svc
            wifi_svc)

set(requires
            hal_layer)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
