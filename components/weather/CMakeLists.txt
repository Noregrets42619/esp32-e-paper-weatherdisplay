idf_component_register(SRCS "src/weather.c" "src/weather_parse.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client mbedtls json esp_timer)
