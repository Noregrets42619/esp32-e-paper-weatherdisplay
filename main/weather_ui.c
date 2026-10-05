#include "weather_ui.h"
#include "color_canvas.h"
#include "fonts/weather_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

// Icons use a 64-unit coordinate system, scaled to current/forecast sizes.
static void icon_line(ColorCanvas *c, int x, int y, int size, int ax, int ay, int bx, int by,
                      enum epd_color color)
{
    canvas_line(c, x + ax * size / 64, y + ay * size / 64,
                x + bx * size / 64, y + by * size / 64, color);
}

static void icon_circle(ColorCanvas *c, int x, int y, int size, int cx, int cy, int radius,
                        enum epd_color color)
{
    canvas_circle(c, x + cx * size / 64, y + cy * size / 64, radius * size / 64, color, 1);
}

static void sun(ColorCanvas *c, int x, int y, int size)
{
    const int rays[][4] = {{32,3,32,11},{32,53,32,61},{3,32,11,32},{53,32,61,32},
                          {11,11,17,17},{47,47,53,53},{11,53,17,47},{47,17,53,11}};
    for (unsigned i = 0; i < sizeof(rays) / sizeof(rays[0]); ++i)
        icon_line(c, x, y, size, rays[i][0], rays[i][1], rays[i][2], rays[i][3], EPD_BLACK);
    icon_circle(c, x, y, size, 32, 32, 19, EPD_BLACK);
    icon_circle(c, x, y, size, 32, 32, 17, EPD_YELLOW);
}

static void moon(ColorCanvas *c, int x, int y, int size)
{
    icon_circle(c, x, y, size, 30, 30, 24, EPD_BLACK);
    icon_circle(c, x, y, size, 30, 30, 22, EPD_YELLOW);
    icon_circle(c, x, y, size, 42, 20, 24, EPD_WHITE);
}

static void cloud(ColorCanvas *c, int x, int y, int size)
{
    icon_circle(c, x, y, size, 16, 32, 13, EPD_BLACK);
    icon_circle(c, x, y, size, 31, 25, 17, EPD_BLACK);
    icon_circle(c, x, y, size, 47, 33, 12, EPD_BLACK);
    canvas_fill_rect(c, x + 16 * size / 64, y + 31 * size / 64, 32 * size / 64, 15 * size / 64, EPD_BLACK);
    icon_circle(c, x, y, size, 16, 32, 11, EPD_WHITE);
    icon_circle(c, x, y, size, 31, 25, 15, EPD_WHITE);
    icon_circle(c, x, y, size, 47, 33, 10, EPD_WHITE);
    canvas_fill_rect(c, x + 16 * size / 64, y + 28 * size / 64, 32 * size / 64, 15 * size / 64, EPD_WHITE);
}

static void weather_icon(ColorCanvas *c, int x, int y, int size, const char *name)
{
    if (!strcmp(name, "clear-day")) { sun(c, x, y, size); return; }
    if (!strcmp(name, "clear-night")) { moon(c, x, y, size); return; }
    if (strstr(name, "partly-cloudy")) {
        if (strstr(name, "night")) moon(c, x + size / 4, y, size * 3 / 4);
        else sun(c, x + size / 4, y, size * 3 / 4);
    }
    if (!strcmp(name, "unknown")) {
        canvas_text_center(c, x, y + size / 4, size, "?", &WeatherSans20, EPD_RED);
        return;
    }
    cloud(c, x, y, size);
    if (!strcmp(name, "rain") || !strcmp(name, "sleet")) {
        for (int i = 0; i < 3; ++i) {
            icon_line(c, x, y, size, 18 + i * 14, 49, 14 + i * 14, 58, EPD_RED);
            icon_line(c, x, y, size, 19 + i * 14, 49, 15 + i * 14, 58, EPD_RED);
        }
    } else if (!strcmp(name, "snow") || !strcmp(name, "hail")) {
        for (int i = 0; i < 3; ++i) {
            int cx = 16 + i * 16;
            icon_line(c, x, y, size, cx - 4, 55, cx + 4, 55, EPD_RED);
            icon_line(c, x, y, size, cx, 51, cx, 59, EPD_RED);
            icon_line(c, x, y, size, cx - 3, 52, cx + 3, 58, EPD_RED);
        }
    } else if (!strcmp(name, "thunderstorm")) {
        for (int i = 0; i < 4; ++i) {
            icon_line(c, x, y, size, 36 + i, 43, 27 + i, 53, EPD_RED);
            icon_line(c, x, y, size, 27 + i, 53, 36 + i, 53, EPD_RED);
            icon_line(c, x, y, size, 36 + i, 53, 26 + i, 63, EPD_RED);
        }
    } else if (!strcmp(name, "fog")) {
        icon_line(c, x, y, size, 10, 52, 54, 52, EPD_BLACK);
        icon_line(c, x, y, size, 16, 59, 48, 59, EPD_BLACK);
    }
}

void weather_ui_render(uint8_t *frame, const WeatherData *data, const char *place)
{
    ColorCanvas c = {.pixels = frame};
    char text[96];
    struct tm date = {0};
    localtime_r(&data->observed_at, &date);
    canvas_clear(&c, EPD_WHITE);
    canvas_fill_rect(&c, 0, 7, 4, 24, EPD_RED);
    canvas_text_fit(&c, 12, 6, 238, place, &WeatherSans20, EPD_BLACK);
    snprintf(text, sizeof(text), "%02d月%02d日 %s", date.tm_mon + 1, date.tm_mday, weekdays[date.tm_wday]);
    canvas_text(&c, 390 - canvas_text_width(text, &WeatherSans13), 1, text, &WeatherSans13, EPD_BLACK);
    snprintf(text, sizeof(text), "数据更新 %02d:%02d", date.tm_hour, date.tm_min);
    canvas_text(&c, 390 - canvas_text_width(text, &WeatherSans13), 19, text, &WeatherSans13, EPD_BLACK);
    canvas_line(&c, 10, 37, 390, 37, EPD_BLACK);

    weather_icon(&c, 12, 42, 62, data->icon);
    canvas_text_center(&c, 8, 105, 76, data->summary, &WeatherSans16, EPD_BLACK);
    snprintf(text, sizeof(text), "%.1f", data->temperature);
    const UiFont *temperature_font = &WeatherSans44;
    if (canvas_text_width(text, temperature_font) > 124) temperature_font = &WeatherSans32;
    canvas_text(&c, 91, 47, text, temperature_font, EPD_BLACK);
    canvas_text(&c, 94 + canvas_text_width(text, temperature_font), 65, "℃", &WeatherSans20, EPD_RED);
    snprintf(text, sizeof(text), "今日 %d° / %d°", (int)lround(data->forecasts[0].temperatureMax),
             (int)lround(data->forecasts[0].temperatureMin));
    canvas_text_fit(&c, 92, 106, 145, text, &WeatherSans13, EPD_BLACK);
    canvas_line(&c, 241, 48, 241, 120, EPD_BLACK);
    snprintf(text, sizeof(text), "湿度 %d%%", (int)lround(data->humidity * 100));
    canvas_text(&c, 253, 48, text, &WeatherSans16, EPD_BLACK);
    snprintf(text, sizeof(text), "气压 %d hPa", data->pressure);
    canvas_text(&c, 253, 77, text, &WeatherSans13, EPD_BLACK);
    snprintf(text, sizeof(text), "%s风 %.1f km/h", deg_to_compass((int)lround(data->wind_bearing)), data->wind_speed * 3.6);
    const UiFont *wind_font = canvas_text_width(text, &WeatherSans13) <= 137 ? &WeatherSans13 : &WeatherSans10;
    canvas_text(&c, 253, 103, text, wind_font, EPD_BLACK);

    enum epd_color rain_background = isfinite(data->precip_probability) && data->precip_probability >= 0.5 ? EPD_RED : EPD_YELLOW;
    enum epd_color rain_text = rain_background == EPD_RED ? EPD_WHITE : EPD_BLACK;
    canvas_fill_rect(&c, 10, 129, 380, 26, rain_background);
    canvas_text(&c, 18, 134, "今日最高降水概率", &WeatherSans13, rain_text);
    if (isfinite(data->precip_probability)) snprintf(text, sizeof(text), "%d%%", (int)lround(data->precip_probability * 100));
    else snprintf(text, sizeof(text), "暂无数据");
    canvas_text(&c, 380 - canvas_text_width(text, &WeatherSans16), 132, text, &WeatherSans16, rain_text);

    canvas_text(&c, 12, 160, "七日天气", &WeatherSans16, EPD_BLACK);
    canvas_text(&c, 286, 163, "最高", &WeatherSans13, EPD_RED);
    canvas_text(&c, 318, 163, "/ 最低 ℃", &WeatherSans13, EPD_BLACK);
    for (int i = 0; i < WEATHER_FORECAST_DAYS; ++i) {
        int left = 4 + i * 56;
        const Forecast *day = &data->forecasts[i];
        localtime_r(&day->time, &date);
        const char *label = i == 0 ? "今天" : i == 1 ? "明天" : weekdays[date.tm_wday];
        if (i == 0) canvas_fill_rect(&c, left + 5, 181, 46, 18, EPD_RED);
        canvas_text_center(&c, left, 182, 56, label, &WeatherSans13, i == 0 ? EPD_WHITE : EPD_BLACK);
        snprintf(text, sizeof(text), "%02d/%02d", date.tm_mon + 1, date.tm_mday);
        canvas_text_center(&c, left, 202, 56, text, &WeatherSans10, EPD_BLACK);
        weather_icon(&c, left + 13, 216, 30, day->icon);
        canvas_text_center(&c, left, 248, 56, day->summary, &WeatherSans13, EPD_BLACK);
        char high[16], low[16];
        snprintf(high, sizeof(high), "%d°", (int)lround(day->temperatureMax));
        snprintf(low, sizeof(low), "%d°", (int)lround(day->temperatureMin));
        const UiFont *font = &WeatherSans13;
        if (canvas_text_width(high, font) + canvas_text_width(low, font) + 6 > 54) font = &WeatherSans10;
        int high_width = canvas_text_width(high, font);
        int width = high_width + 6 + canvas_text_width(low, font);
        int tx = left + (56 - width) / 2;
        canvas_text(&c, tx, 268, high, font, EPD_RED);
        canvas_text(&c, tx + high_width + 6, 268, low, font, EPD_BLACK);
        if (i) canvas_line(&c, left, 183, left, 281, EPD_BLACK);
    }
    canvas_line(&c, 10, 285, 390, 285, EPD_BLACK);
    canvas_text(&c, 12, 286, "数据：Open-Meteo.com", &WeatherSans10, EPD_BLACK);
    canvas_text(&c, 342, 286, "北京时间", &WeatherSans10, EPD_BLACK);
}
