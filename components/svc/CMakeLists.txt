set(src_dirs
            filter_lowpass_svc
            wifi_svc
            net_svc)

set(include_dirs
            filter_lowpass_svc
            wifi_svc
            net_svc)

set(requires
            hal_layer
            esp_http_client
            lwip
            cjson)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
