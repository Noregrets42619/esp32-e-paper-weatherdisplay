#pragma once

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#define WEATHER_FORECAST_DAYS 7

typedef struct {
    time_t time;
    char summary[50];
    char icon[20];
    double temperatureMax;
    double temperatureMin;
} Forecast;

typedef struct {
    time_t observed_at;
    char summary[50];
    char icon[20];
    double temperature;
    double humidity;             // Fraction, 0..1
    int pressure;                // Sea-level hPa
    double wind_speed;           // m/s
    double wind_bearing;         // Degrees
    double precip_probability;  // Today's maximum probability, fraction or NAN
    Forecast forecasts[WEATHER_FORECAST_DAYS];
} WeatherData;

extern WeatherData weather;

// The caller's output is unchanged on malformed/incomplete responses.
esp_err_t weather_parse_json(const char* json, WeatherData* output);
esp_err_t get_current_weather(void);
const char* deg_to_compass(int degrees);
