#ifndef EPD4IN2G_H
#define EPD4IN2G_H

#include "esp_err.h"
#include <stdint.h>

#define EPD_WIDTH 400
#define EPD_HEIGHT 300
#define EPD_FRAME_BYTES (EPD_WIDTH * EPD_HEIGHT / 8)
#define EPD_COLOR_FRAME_BYTES (EPD_WIDTH * EPD_HEIGHT / 4)

// G panel: each pixel is 00 black, 01 white, 10 yellow, or 11 red (MSB first).
enum epd_color { EPD_BLACK = 0, EPD_WHITE = 1, EPD_YELLOW = 2, EPD_RED = 3 };

esp_err_t epd4in2g_init(void);
// Existing painter: 1 bit per pixel, 0 black / 1 white; expanded during transfer.
esp_err_t epd4in2g_display_mono(const uint8_t *frame);
esp_err_t epd4in2g_display_color(const uint8_t *frame);
esp_err_t epd4in2g_sleep(void);
// Convert EPD_FRAME_BYTES of monochrome pixels to EPD_COLOR_FRAME_BYTES.
void epd4in2g_convert_mono(const uint8_t *mono, uint8_t *color);

#endif
