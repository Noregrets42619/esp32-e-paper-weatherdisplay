#include "ota.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "esp_http_ota.h"

static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
{
    static const char* TAG = "_http_event_handler";
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void init_ota_button(void)
{
#ifdef CONFIG_ENABLE_OTA_BUTTON
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << GPIO_NUM_39,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
#endif
}

uint8_t check_if_ota_button_pressed(void)
{
#ifdef CONFIG_ENABLE_OTA_BUTTON
    return gpio_get_level(GPIO_NUM_39) == 0;
#else
    return 0;
#endif
}

void ota_task(void* pvParameter)
{
    static const char* TAG = "simple_ota_example_task";
    ESP_LOGI(TAG, "Starting OTA example...");

    esp_http_client_config_t config = {
        .url = CONFIG_OTA_URL,
        .event_handler = _http_event_handler,
    };

    esp_err_t ret = esp_http_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware Upgrades Failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}