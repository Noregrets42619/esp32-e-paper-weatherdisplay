// Host verification and preview using the same renderer linked into the firmware.
#include "color_canvas.h"
#include "weather_ui.h"
#include "fonts/weather_font.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t frame[EPD_COLOR_FRAME_BYTES + 16];

static unsigned pixel(int x, int y)
{
    int index = y * EPD_WIDTH + x;
    return (frame[index / 4] >> (6 - (index % 4) * 2)) & 3;
}

static void check_canvas(void)
{
    ColorCanvas canvas = {.pixels = frame};
    memset(frame + EPD_COLOR_FRAME_BYTES, 0xa5, 16);
    canvas_clear(&canvas, EPD_WHITE);
    for (int color = EPD_BLACK; color <= EPD_RED; ++color) canvas_pixel(&canvas, color, 0, color);
    assert(frame[0] == 0x1b); // black / white / yellow / red, MSB first
    canvas_fill_rect(&canvas, -5, -8, 10, 10, EPD_YELLOW);
    assert(pixel(0, 0) == EPD_YELLOW && pixel(4, 1) == EPD_YELLOW && pixel(5, 1) == EPD_WHITE);
    canvas_line(&canvas, 399, 299, 390, 299, EPD_RED);
    assert(pixel(399, 299) == EPD_RED && pixel(390, 299) == EPD_RED);
    canvas_line(&canvas, 0, 2, 0, 12, EPD_BLACK);
    assert(pixel(0, 2) == EPD_BLACK && pixel(0, 12) == EPD_BLACK);
    canvas_pixel(&canvas, 400, 300, EPD_BLACK);
    canvas_fill_rect(&canvas, 398, 298, 100, 100, EPD_BLACK);
    canvas_text(&canvas, 398, 295, "成都市郫都区", &WeatherSans20, EPD_RED);
    for (int i = 0; i < 16; ++i) assert(frame[EPD_COLOR_FRAME_BYTES + i] == 0xa5);
    assert(canvas_text_width("成都市郫都区", &WeatherSans20) == 120);
    assert(canvas_text_width("\xe4", &WeatherSans13) == canvas_text_width("?", &WeatherSans13));
    assert(canvas_text_width("\xf0\x80\x80\x80", &WeatherSans13) == canvas_text_width("?", &WeatherSans13));
    canvas_clear(&canvas, EPD_WHITE);
    canvas_text_fit(&canvas, 10, 10, 52, "成都市郫都区", &WeatherSans13, EPD_BLACK);
    for (int y = 10; y < 30; ++y) for (int x = 63; x < 90; ++x) assert(pixel(x, y) == EPD_WHITE);
}

static char *fixture_json(int code, int is_day)
{
    char json[4096];
    snprintf(json, sizeof(json),
        "{\"current_units\":{\"time\":\"unixtime\",\"temperature_2m\":\"°C\","
        "\"relative_humidity_2m\":\"%%\",\"pressure_msl\":\"hPa\",\"wind_speed_10m\":\"m/s\"},"
        "\"daily_units\":{\"time\":\"unixtime\",\"temperature_2m_min\":\"°C\","
        "\"temperature_2m_max\":\"°C\",\"precipitation_probability_max\":\"%%\"},"
        "\"current\":{\"time\":1791178200,\"temperature_2m\":23.6,\"relative_humidity_2m\":78,"
        "\"pressure_msl\":1012,\"wind_speed_10m\":2.3,\"wind_direction_10m\":45,\"weather_code\":%d,\"is_day\":%d},"
        "\"daily\":{\"time\":[1791129600,1791216000,1791302400,1791388800,1791475200,1791561600,1791648000],"
        "\"temperature_2m_min\":[18,17,17,18,16,17,18],\"temperature_2m_max\":[26,25,23,24,22,24,26],"
        "\"weather_code\":[2,0,61,95,3,45,0],\"precipitation_probability_max\":[80,10,90,85,20,5,0]}}", code, is_day);
    return strdup(json);
}

static void check_parser(void)
{
    const int codes[] = {0,1,2,3,45,48,51,53,55,56,57,61,63,65,66,67,71,73,75,77,80,81,82,85,86,95,96,99,999};
    for (unsigned i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        char *json = fixture_json(codes[i], 1);
        WeatherData data;
        assert(weather_parse_json(json, &data) == ESP_OK);
        assert((unsigned char)data.summary[0] >= 0x80); // Chinese, not legacy English
        assert(canvas_text_width(data.summary, &WeatherSans13) <= 52);
        assert(canvas_text_width(data.summary, &WeatherSans16) <= 76);
        // Every translated character is covered, including four-character conditions.
        for (size_t j = 0; j < strlen(data.summary); j += 3) {
            unsigned char *p = (unsigned char *)data.summary + j;
            uint32_t codepoint = ((p[0] & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
            int found = 0;
            for (int k = 0; k < WeatherSans13.count; ++k) found |= WeatherSans13.glyphs[k].codepoint == codepoint;
            assert(found);
        }
        free(json);
    }
    assert(!strcmp(deg_to_compass(0), "北"));
    assert(!strcmp(deg_to_compass(360), "北"));
    assert(!strcmp(deg_to_compass(45), "东北"));
    char *json = fixture_json(0, 0);
    WeatherData data;
    assert(weather_parse_json(json, &data) == ESP_OK && !strcmp(data.icon, "clear-night"));
    free(json);
}

static void output(const char *directory, const char *name, const WeatherData *data, const char *place)
{
    weather_ui_render(frame, data, place);
    for (int i = 0; i < 16; ++i) assert(frame[EPD_COLOR_FRAME_BYTES + i] == 0xa5);
    unsigned counts[4] = {0};
    for (int y = 0; y < EPD_HEIGHT; ++y) for (int x = 0; x < EPD_WIDTH; ++x) ++counts[pixel(x, y)];
    for (int i = 0; i < 4; ++i) assert(counts[i] > 0);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.bin", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(frame, 1, EPD_COLOR_FRAME_BYTES, file) == EPD_COLOR_FRAME_BYTES);
    fclose(file);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    setenv("TZ", "CST-8", 1);
    tzset();
    check_canvas();
    check_parser();
    char *json = fixture_json(2, 1);
    WeatherData data;
    assert(weather_parse_json(json, &data) == ESP_OK);
    free(json);
    output(argv[1], "weather-color-zh", &data, "成都市郫都区");
    strcpy(data.summary, "晴"); strcpy(data.icon, "clear-night");
    data.precip_probability = NAN;
    output(argv[1], "weather-night-zh", &data, "成都市郫都区");
    data.temperature = -100.0; data.wind_speed = 150; data.wind_bearing = 22.5;
    for (int i = 0; i < WEATHER_FORECAST_DAYS; ++i) {
        strcpy(data.forecasts[i].summary, "雷暴冰雹"); strcpy(data.forecasts[i].icon, "hail");
        data.forecasts[i].temperatureMax = -99; data.forecasts[i].temperatureMin = -100;
    }
    output(argv[1], "weather-extreme-zh", &data, "成都市郫都区成都市郫都区成都市郫都区");
    puts("PASS: color packing, clipped drawing, UTF-8, Chinese WMO labels, forecasts and render guards");
    return 0;
}
