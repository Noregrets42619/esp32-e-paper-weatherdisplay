#include "weather.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

WeatherData weather;
static const char* TAG = "open_meteo";

static bool coordinate(const char* text, double min, double max, double* value)
{
    char* end;
    *value = strtod(text, &end);
    return end != text && *end == '\0' && isfinite(*value) && *value >= min && *value <= max;
}

static esp_err_t fetch(const char* url, char* buffer, size_t capacity)
{
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .user_agent = "WT32-ETH01-WeatherDisplay/1.0",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    int64_t deadline = esp_timer_get_time() + 30000000;
    esp_err_t error = esp_http_client_open(client, 0);
    if (error != ESP_OK) goto done;
    int64_t length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (length < 0 || status != 200) {
        ESP_LOGE(TAG, "Weather request failed (HTTP %d)", status);
        error = ESP_FAIL;
        goto done;
    }
    if (length >= (int64_t)capacity) { error = ESP_ERR_INVALID_SIZE; goto done; }
    size_t used = 0;
    while (!esp_http_client_is_complete_data_received(client)) {
        if (used == capacity - 1) { error = ESP_ERR_INVALID_SIZE; break; }
        if (esp_timer_get_time() >= deadline) { error = ESP_ERR_TIMEOUT; break; }
        int received = esp_http_client_read(client, buffer + used, capacity - 1 - used);
        if (received < 0) { error = ESP_FAIL; break; }
        if (received == 0) {
            if (!esp_http_client_is_complete_data_received(client)) error = ESP_FAIL;
            break;
        }
        used += received;
    }
    buffer[used] = '\0';
done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return error;
}

esp_err_t get_current_weather(void)
{
    double latitude, longitude;
    if (!coordinate(CONFIG_LATITUDE, -90, 90, &latitude) ||
        !coordinate(CONFIG_LONGITUDE, -180, 180, &longitude)) return ESP_ERR_INVALID_ARG;
    // Only IANA timezone characters; prevent accidental query injection.
    const char* zone = CONFIG_WEATHER_TIMEZONE;
    if (!*zone || strspn(zone, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_+-") != strlen(zone))
        return ESP_ERR_INVALID_ARG;
    char url[768];
    int length = snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
        "&current=temperature_2m,relative_humidity_2m,pressure_msl,wind_speed_10m,wind_direction_10m,weather_code,is_day"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
        "&timezone=%s&forecast_days=7&timeformat=unixtime&wind_speed_unit=ms&temperature_unit=celsius",
        latitude, longitude, zone);
    if (length < 0 || length >= sizeof(url)) return ESP_ERR_INVALID_SIZE;
    char* response = malloc(16384);
    if (!response) return ESP_ERR_NO_MEM;
    esp_err_t result = fetch(url, response, 16384);
    if (result == ESP_OK) result = weather_parse_json(response, &weather);
    free(response);
    if (result == ESP_OK) ESP_LOGI(TAG, "Received current weather and seven-day forecast for %s", CONFIG_PLACE_NAME);
    else ESP_LOGW(TAG, "Weather update failed: %s", esp_err_to_name(result));
    return result;
}
