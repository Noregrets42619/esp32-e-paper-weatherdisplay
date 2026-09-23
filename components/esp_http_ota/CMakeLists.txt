idf_component_register(SRCS "src/esp_http_ota.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client
    PRIV_REQUIRES app_update)
