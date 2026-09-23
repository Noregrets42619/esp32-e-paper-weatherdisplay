idf_component_register(SRCS "src/epdif.c" "src/epdpaint.c" "src/epd4in2b.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_system)
