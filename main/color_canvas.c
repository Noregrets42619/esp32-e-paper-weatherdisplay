#include "color_canvas.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static uint32_t next_codepoint(const char **cursor)
{
    const unsigned char *p = (const unsigned char *)*cursor;
    if (!*p) return 0;
    uint32_t value;
    unsigned count;
    uint32_t minimum;
    if (*p < 0x80) { *cursor += 1; return *p; }
    if (*p >= 0xc2 && *p <= 0xdf) { count = 2; value = *p & 31; minimum = 0x80; }
    else if (*p >= 0xe0 && *p <= 0xef) { count = 3; value = *p & 15; minimum = 0x800; }
    else if (*p >= 0xf0 && *p <= 0xf4) { count = 4; value = *p & 7; minimum = 0x10000; }
    else { *cursor += 1; return 0xfffd; }
    for (unsigned i = 1; i < count; ++i) {
        if ((p[i] & 0xc0) != 0x80) { *cursor += 1; return 0xfffd; }
        value = (value << 6) | (p[i] & 63);
    }
    *cursor += count;
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0xfffd;
    return value;
}

static const UiGlyph *glyph(const UiFont *font, uint32_t codepoint)
{
    int lo = 0, hi = font->count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (font->glyphs[mid].codepoint == codepoint) return &font->glyphs[mid];
        if (font->glyphs[mid].codepoint < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }
    return codepoint == '?' ? NULL : glyph(font, '?');
}

void canvas_clear(ColorCanvas *canvas, enum epd_color color)
{
    memset(canvas->pixels, (uint8_t)color * 0x55, EPD_COLOR_FRAME_BYTES);
}

void canvas_pixel(ColorCanvas *canvas, int x, int y, enum epd_color color)
{
    if ((unsigned)x >= EPD_WIDTH || (unsigned)y >= EPD_HEIGHT) return;
    unsigned index = (unsigned)y * EPD_WIDTH + (unsigned)x;
    unsigned shift = (3 - index % 4) * 2;
    uint8_t *byte = &canvas->pixels[index / 4];
    *byte = (*byte & ~(3u << shift)) | ((unsigned)color << shift);
}

void canvas_fill_rect(ColorCanvas *canvas, int x, int y, int w, int h, enum epd_color color)
{
    int right = x + w, bottom = y + h;
    if (right > EPD_WIDTH) right = EPD_WIDTH;
    if (bottom > EPD_HEIGHT) bottom = EPD_HEIGHT;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    for (int py = y; py < bottom; ++py)
        for (int px = x; px < right; ++px) canvas_pixel(canvas, px, py, color);
}

void canvas_line(ColorCanvas *canvas, int x0, int y0, int x1, int y1, enum epd_color color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        canvas_pixel(canvas, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void canvas_circle(ColorCanvas *canvas, int x, int y, int radius, enum epd_color color, int filled)
{
    int px = radius, py = 0, error = 1 - radius;
    while (px >= py) {
        if (filled) {
            canvas_line(canvas, x - px, y + py, x + px, y + py, color);
            canvas_line(canvas, x - px, y - py, x + px, y - py, color);
            canvas_line(canvas, x - py, y + px, x + py, y + px, color);
            canvas_line(canvas, x - py, y - px, x + py, y - px, color);
        } else {
            canvas_pixel(canvas, x + px, y + py, color); canvas_pixel(canvas, x - px, y + py, color);
            canvas_pixel(canvas, x + px, y - py, color); canvas_pixel(canvas, x - px, y - py, color);
            canvas_pixel(canvas, x + py, y + px, color); canvas_pixel(canvas, x - py, y + px, color);
            canvas_pixel(canvas, x + py, y - px, color); canvas_pixel(canvas, x - py, y - px, color);
        }
        ++py;
        if (error < 0) error += 2 * py + 1;
        else { --px; error += 2 * (py - px) + 1; }
    }
}

int canvas_text_width(const char *text, const UiFont *font)
{
    int width = 0;
    while (*text) {
        const UiGlyph *g = glyph(font, next_codepoint(&text));
        if (g) width += g->advance;
    }
    return width;
}

void canvas_text(ColorCanvas *canvas, int x, int y, const char *text,
                 const UiFont *font, enum epd_color color)
{
    while (*text) {
        const UiGlyph *g = glyph(font, next_codepoint(&text));
        if (!g) continue;
        for (int gy = 0; gy < g->height; ++gy) {
            for (int gx = 0; gx < g->width; ++gx) {
                unsigned bit = gy * g->width + gx;
                if (font->bitmap[g->offset + bit / 8] & (0x80 >> (bit % 8)))
                    canvas_pixel(canvas, x + g->x_offset + gx, y + g->y_offset + gy, color);
            }
        }
        x += g->advance;
    }
}

void canvas_text_center(ColorCanvas *canvas, int x, int y, int width, const char *text,
                        const UiFont *font, enum epd_color color)
{
    canvas_text(canvas, x + (width - canvas_text_width(text, font)) / 2, y, text, font, color);
}

void canvas_text_fit(ColorCanvas *canvas, int x, int y, int width, const char *text,
                     const UiFont *font, enum epd_color color)
{
    if (canvas_text_width(text, font) <= width) { canvas_text(canvas, x, y, text, font, color); return; }
    int remaining = width - canvas_text_width("…", font);
    while (*text) {
        const char *end = text;
        const UiGlyph *g = glyph(font, next_codepoint(&end));
        int advance = g ? g->advance : 0;
        if (advance > remaining) break;
        char character[5] = {0};
        memcpy(character, text, (size_t)(end - text));
        canvas_text(canvas, x, y, character, font, color);
        x += advance; remaining -= advance; text = end;
    }
    if (width >= canvas_text_width("…", font)) canvas_text(canvas, x, y, "…", font, color);
}
