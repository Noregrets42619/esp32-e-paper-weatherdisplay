idf_component_register(SRCS "main.c" "ota.c"
    INCLUDE_DIRS "."
    REQUIRES weather epd4in2b esp_http_ota esp_wifi esp_event esp_netif
             nvs_flash lwip esp_driver_gpio esp_system esp_hw_support)
