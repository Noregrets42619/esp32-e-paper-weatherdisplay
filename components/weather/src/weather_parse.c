#include "weather.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool number(const cJSON* value, double min, double max, double* output)
{
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble) ||
        value->valuedouble < min || value->valuedouble > max) return false;
    *output = value->valuedouble;
    return true;
}

static bool field(const cJSON* object, const char* key, double min, double max, double* value)
{
    return number(cJSON_GetObjectItemCaseSensitive(object, key), min, max, value);
}

static void describe(int code, bool day, char* summary, size_t summary_size,
                     char* icon, size_t icon_size)
{
    const char* text = "Unknown";
    const char* symbol = "unknown";
    switch (code) {
    case 0: text = "Clear"; symbol = day ? "clear-day" : "clear-night"; break;
    case 1: text = "Mainly clear"; symbol = day ? "clear-day" : "clear-night"; break;
    case 2: text = "Partly cloudy"; symbol = day ? "partly-cloudy-day" : "partly-cloudy-night"; break;
    case 3: text = "Overcast"; symbol = "cloudy"; break;
    case 45: case 48: text = "Fog"; symbol = "fog"; break;
    case 51: case 53: case 55: text = "Drizzle"; symbol = "rain"; break;
    case 56: case 57: text = "Freezing drizzle"; symbol = "sleet"; break;
    case 61: case 63: case 65: text = "Rain"; symbol = "rain"; break;
    case 66: case 67: text = "Freezing rain"; symbol = "sleet"; break;
    case 71: case 73: case 75: case 77: text = "Snow"; symbol = "snow"; break;
    case 80: case 81: case 82: text = "Rain showers"; symbol = "rain"; break;
    case 85: case 86: text = "Snow showers"; symbol = "snow"; break;
    case 95: text = "Thunderstorm"; symbol = "rain"; break;
    case 96: case 99: text = "Storm with hail"; symbol = "sleet"; break;
    }
    snprintf(summary, summary_size, "%s", text);
    snprintf(icon, icon_size, "%s", symbol);
}

static bool units_match(const cJSON* units, const char* key, const char* expected)
{
    const cJSON* unit = cJSON_GetObjectItemCaseSensitive(units, key);
    return cJSON_IsString(unit) && strcmp(unit->valuestring, expected) == 0;
}

esp_err_t weather_parse_json(const char* json, WeatherData* output)
{
    if (!json || !output) return ESP_ERR_INVALID_ARG;
    cJSON* root = cJSON_Parse(json);
    if (!root) return ESP_FAIL;
    esp_err_t result = ESP_FAIL;
    WeatherData parsed = {0};
    const cJSON* current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON* daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    const cJSON* units = cJSON_GetObjectItemCaseSensitive(root, "current_units");
    const cJSON* daily_units = cJSON_GetObjectItemCaseSensitive(root, "daily_units");
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "error")) ||
        !units_match(units, "time", "unixtime") ||
        !units_match(units, "temperature_2m", "\xc2\xb0" "C") ||
        !units_match(units, "relative_humidity_2m", "%") ||
        !units_match(units, "pressure_msl", "hPa") ||
        !units_match(units, "wind_speed_10m", "m/s") ||
        !units_match(daily_units, "time", "unixtime") ||
        !units_match(daily_units, "temperature_2m_min", "\xc2\xb0" "C") ||
        !units_match(daily_units, "temperature_2m_max", "\xc2\xb0" "C")) goto done;

    double timestamp, humidity, pressure, code, is_day;
    if (!field(current, "time", 1577836800, 4102444800, &timestamp) ||
        !field(current, "temperature_2m", -100, 70, &parsed.temperature) ||
        !field(current, "relative_humidity_2m", 0, 100, &humidity) ||
        !field(current, "pressure_msl", 800, 1200, &pressure) ||
        !field(current, "wind_speed_10m", 0, 150, &parsed.wind_speed) ||
        !field(current, "wind_direction_10m", 0, 360, &parsed.wind_bearing) ||
        !field(current, "weather_code", 0, 999, &code) || floor(code) != code ||
        !field(current, "is_day", 0, 1, &is_day) || floor(is_day) != is_day) goto done;
    parsed.observed_at = (time_t)timestamp;
    parsed.humidity = humidity / 100.0;
    parsed.pressure = (int)lround(pressure);
    describe((int)code, is_day != 0, parsed.summary, sizeof(parsed.summary),
             parsed.icon, sizeof(parsed.icon));

    const char* keys[] = {"time", "temperature_2m_min", "temperature_2m_max", "weather_code"};
    const cJSON* arrays[4];
    for (size_t i = 0; i < 4; ++i) {
        arrays[i] = cJSON_GetObjectItemCaseSensitive(daily, keys[i]);
        if (!cJSON_IsArray(arrays[i]) || cJSON_GetArraySize(arrays[i]) != WEATHER_FORECAST_DAYS) goto done;
    }
    for (int i = 0; i < WEATHER_FORECAST_DAYS; ++i) {
        Forecast* day = &parsed.forecasts[i];
        if (!number(cJSON_GetArrayItem(arrays[0], i), 1577836800, 4102444800, &timestamp) ||
            !number(cJSON_GetArrayItem(arrays[1], i), -100, 70, &day->temperatureMin) ||
            !number(cJSON_GetArrayItem(arrays[2], i), -100, 70, &day->temperatureMax) ||
            day->temperatureMin > day->temperatureMax ||
            !number(cJSON_GetArrayItem(arrays[3], i), 0, 999, &code) || floor(code) != code) goto done;
        // Unix seconds are UTC. localtime_r() applies the configured UTC+8 once.
        day->time = (time_t)timestamp;
        if (i > 0 && day->time <= parsed.forecasts[i - 1].time) goto done;
        describe((int)code, true, day->summary, sizeof(day->summary), day->icon, sizeof(day->icon));
    }
    if (parsed.observed_at < parsed.forecasts[0].time ||
        parsed.observed_at >= parsed.forecasts[1].time) goto done;

    // Some models omit precipitation probability. Show N/A, never a false 0%.
    parsed.precip_probability = NAN;
    double probability;
    const cJSON* rain = cJSON_GetObjectItemCaseSensitive(daily, "precipitation_probability_max");
    if (units_match(daily_units, "precipitation_probability_max", "%") &&
        number(cJSON_GetArrayItem(rain, 0), 0, 100, &probability)) {
        parsed.precip_probability = probability / 100.0;
    }
    *output = parsed;
    result = ESP_OK;
done:
    cJSON_Delete(root);
    return result;
}

const char* deg_to_compass(int degrees)
{
    const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    int sector = (int)floor(degrees / 22.5 + 0.5);
    return directions[(sector % 16 + 16) % 16];
}
