#pragma once

#include <stdint.h>
#include "epd4in2g.h"

typedef struct {
    uint32_t codepoint;
    uint32_t offset;
    uint8_t width, height, advance;
    int8_t x_offset, y_offset;
} UiGlyph;

typedef struct {
    const UiGlyph *glyphs;
    const uint8_t *bitmap;
    uint16_t count;
    uint8_t height;
} UiFont;

typedef struct { uint8_t *pixels; } ColorCanvas;

void canvas_clear(ColorCanvas *canvas, enum epd_color color);
void canvas_pixel(ColorCanvas *canvas, int x, int y, enum epd_color color);
void canvas_fill_rect(ColorCanvas *canvas, int x, int y, int w, int h, enum epd_color color);
void canvas_line(ColorCanvas *canvas, int x0, int y0, int x1, int y1, enum epd_color color);
void canvas_circle(ColorCanvas *canvas, int x, int y, int radius, enum epd_color color, int filled);
int canvas_text_width(const char *text, const UiFont *font);
void canvas_text(ColorCanvas *canvas, int x, int y, const char *text,
                 const UiFont *font, enum epd_color color);
void canvas_text_center(ColorCanvas *canvas, int x, int y, int width, const char *text,
                        const UiFont *font, enum epd_color color);
// Clip text at a UTF-8 character boundary and add an ellipsis if needed.
void canvas_text_fit(ColorCanvas *canvas, int x, int y, int width, const char *text,
                     const UiFont *font, enum epd_color color);
