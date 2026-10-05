#pragma once
#include "weather.h"
#include <stdint.h>

// Buffer must hold EPD_COLOR_FRAME_BYTES; the caller sets the local timezone.
void weather_ui_render(uint8_t *frame, const WeatherData *data, const char *place);
