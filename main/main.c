#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include <time.h>
#include <assert.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>



#include "weather.h"
#include <math.h>

#include "epd4in2b.h"

#include "epdpaint.h"

#include "icons.h"

#include "ubuntu10.h"
#include "ubuntu12.h"
#include "ubuntu14.h"
#include "ubuntu16.h"
#include "ubuntu18.h"
#include "ubuntu20.h"
#include "ubuntu22.h"
#include "ubuntu24.h"
#include "ubuntu8.h"

#include "ota.h"

#define COLORED 1
#define UNCOLORED 0

/* The project use simple WiFi configuration that you can set via 'idf.py menuconfig'.*/

/* FreeRTOS event group to signal when we are connected & ready to make a CURRENT_WEATHER_REQUEST */
EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Variable holding number of times ESP32 restarted since first boot.
* It is placed into RTC memory using RTC_DATA_ATTR and
* maintains its value when ESP32 wakes from deep sleep.
*/
RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static time_t time_updated = 0;

/**
 * place times you want your display to be updated in this array
 * examples:
 * 7 * 60 + 0: 7:00
 * 7 * 60 + 10: 7:10
 * 10 * 60 + 40: 10:40
 * 21 * 60 + 30: 21:30
 * enz.
 */
static int update_times[] = {
    7 * 60 + 0, 7 * 60 + 10, 7 * 60 + 20, 7 * 60 + 30, 7 * 60 + 40, 7 * 60 + 50,
    8 * 60 + 0, 8 * 60 + 10, 8 * 60 + 20, 8 * 60 + 30, 8 * 60 + 40, 8 * 60 + 50,
    9 * 60 + 0, 9 * 60 + 10, 9 * 60 + 20, 9 * 60 + 30, 9 * 60 + 40, 9 * 60 + 50,
    10 * 60 + 0, 10 * 60 + 10, 10 * 60 + 20, 10 * 60 + 30, 10 * 60 + 40, 10 * 60 + 50,
    11 * 60 + 0, 11 * 60 + 10, 11 * 60 + 20, 11 * 60 + 30, 11 * 60 + 40, 11 * 60 + 50,
    12 * 60 + 0, 12 * 60 + 10, 12 * 60 + 20, 12 * 60 + 30, 12 * 60 + 40, 12 * 60 + 50,
    13 * 60 + 0, 13 * 60 + 10, 13 * 60 + 20, 13 * 60 + 30, 13 * 60 + 40, 13 * 60 + 50,
    14 * 60 + 0, 14 * 60 + 10, 14 * 60 + 20, 14 * 60 + 30, 14 * 60 + 40, 14 * 60 + 50,
    15 * 60 + 0, 15 * 60 + 10, 15 * 60 + 20, 15 * 60 + 30, 15 * 60 + 40, 15 * 60 + 50,
    16 * 60 + 0, 16 * 60 + 10, 16 * 60 + 20, 16 * 60 + 30, 16 * 60 + 40, 16 * 60 + 50,
    17 * 60 + 0, 17 * 60 + 10, 17 * 60 + 20, 17 * 60 + 30, 17 * 60 + 40, 17 * 60 + 50,
    18 * 60 + 0, 18 * 60 + 10, 18 * 60 + 20, 18 * 60 + 30, 18 * 60 + 40, 18 * 60 + 50,
    19 * 60 + 0, 19 * 60 + 10, 19 * 60 + 20, 19 * 60 + 30, 19 * 60 + 40, 19 * 60 + 50,
    20 * 60 + 0, 20 * 60 + 10, 20 * 60 + 20, 20 * 60 + 30, 20 * 60 + 40, 20 * 60 + 50,
    21 * 60 + 0, 21 * 60 + 10, 21 * 60 + 20, 21 * 60 + 30, 21 * 60 + 40, 21 * 60 + 50,
    22 * 60 + 0, 22 * 60 + 10, 22 * 60 + 20, 22 * 60 + 30, 22 * 60 + 40, 22 * 60 + 50
};

static void event_handler(void* ctx, esp_event_base_t base, int32_t event_id, void* event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_connect();
    }
}

static int initialise_wifi(void)
{
    static const char* TAG = "initialise_wifi";
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* netif = esp_netif_create_default_wifi_sta();
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, CONFIG_ESP_DNS_NAME));
    wifi_event_group = xEventGroupCreate();
    assert(wifi_event_group != NULL);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .bssid_set = false,
        }
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t result = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 10000 / portTICK_PERIOD_MS);

    if (result != CONNECTED_BIT) {
        ESP_LOGE(TAG, "WiFi not connected.");
        return 1;
    } else {
        return 0;
    }
}

static void deinitialize_wifi()
{
    ESP_ERROR_CHECK(esp_wifi_stop());
}

static void initialize_sntp(void)
{
    static const char* TAG = "initialize_sntp";
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

static bool obtain_time(void)
{
    static const char* TAG = "obtain_time";
    if (!(xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true,
                             pdMS_TO_TICKS(10000)) & CONNECTED_BIT)) return false;
    initialize_sntp();

    for (int retry = 0; retry < 10; ++retry) {
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) return true;
        ESP_LOGI(TAG, "Waiting for NTP (%d/10)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGW(TAG, "NTP synchronization timed out");
    return false;
}

static void weather_to_display(void)
{
    static const char* TAG = "weather_to_display_task";

    time_t now;
    struct tm timeinfo = {0};

    char tmp_buff[80];

    if (epd4in2b_init() != 0) {
        ESP_LOGE(TAG, "e-Paper init failed");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        return;
    }
    ESP_LOGI(TAG, "e-Paper initialized");

    clear_frame();

    unsigned char* frame_black = (unsigned char*)malloc(400 * 300 / 8);

    if (frame_black == NULL) {
        ESP_LOGE(TAG, "Cannot allocate display buffer");
        return;
    }

    paint(frame_black, 400, 300);

    clear(UNCOLORED);

    // Current weather
    const tImage* image = NULL;

    if (strcmp(weather.icon, "clear-day") == 0) {
        image = &widaysunny;
    } else if (strcmp(weather.icon, "clear-night") == 0) {
        image = &winightclear;
    } else if (strcmp(weather.icon, "rain") == 0) {
        image = &wirain;
    } else if (strcmp(weather.icon, "snow") == 0) {
        image = &wisnow;
    } else if (strcmp(weather.icon, "sleet") == 0) {
        image = &wisleet;
    } else if (strcmp(weather.icon, "wind") == 0) {
        image = &wistrongwind;
    } else if (strcmp(weather.icon, "fog") == 0) {
        image = &wifog;
    } else if (strcmp(weather.icon, "cloudy") == 0) {
        image = &wicloudy;
    } else if (strcmp(weather.icon, "partly-cloudy-day") == 0) {
        image = &widaycloudy;
    } else if (strcmp(weather.icon, "partly-cloudy-night") == 0) {
        image = &winightaltcloudy;
    }

    if (image != NULL) {
        draw_bitmap_mono_in_center(2, 0, 500, 40, image);
    }

    sprintf(tmp_buff, "%0.1f C", weather.temperature);
    draw_string_in_grid_align_center(3, 0, 400, 45, tmp_buff, &Ubuntu24);

    draw_string_in_grid_align_center(2, 1, 400, 65, weather.summary, &Ubuntu12);

    sprintf(tmp_buff, "Humidity: %d%%", (int)(weather.humidity * 100));
    draw_string_in_grid_align_center(2, 1, 400, 85, tmp_buff, &Ubuntu12);

    sprintf(tmp_buff, "Pressure:%d hPa", weather.pressure);
    draw_string_in_grid_align_center(2, 1, 400, 105, tmp_buff, &Ubuntu12);

    sprintf(tmp_buff, "Wind :%d km/h (%s)", (int)round(weather.wind_speed * 3.6), deg_to_compass(weather.wind_bearing));
    draw_string_in_grid_align_center(2, 1, 400, 125, tmp_buff, &Ubuntu12);

    if (isfinite(weather.precip_probability)) {
        snprintf(tmp_buff, sizeof(tmp_buff), "Precip today (max): %d%%", (int)round(weather.precip_probability * 100));
    } else {
        snprintf(tmp_buff, sizeof(tmp_buff), "Precip today (max): N/A");
    }
    draw_string_in_grid_align_center(2, 1, 400, 145, tmp_buff, &Ubuntu12);

    for (size_t i = 0; i < WEATHER_FORECAST_DAYS; i++) {
        struct tm timeinfo = {0};
        setenv("TZ", CONFIG_DISPLAY_TIMEZONE, 1);
        tzset();
        localtime_r(&weather.forecasts[i].time, &timeinfo);
        char day[20];
        char date[20];
        strftime(date, sizeof(date), "%d - %m", &timeinfo);
        strftime(day, sizeof(date), "%A", &timeinfo);

        if (i == 0) {
            sprintf(day, "Today");
        }

        if (i == 1) {
            sprintf(day, "Tomorrow");
        }

        draw_string_in_grid_align_center(7, i, 400, 210, day, &Ubuntu10);

        draw_string_in_grid_align_center(7, i, 400, 225, date, &Ubuntu10);

        sprintf(tmp_buff, "%d - %d C", (int)round(weather.forecasts[i].temperatureMin), (int)round(weather.forecasts[i].temperatureMax));
        draw_string_in_grid_align_center(7, i, 400, 240, tmp_buff, &Ubuntu10);

        const tImage* forecast_image = NULL;

        if (strcmp(weather.forecasts[i].icon, "clear-day") == 0) {
            forecast_image = &daysunny;
        } else if (strcmp(weather.forecasts[i].icon, "clear-night") == 0) {
            forecast_image = &nightclear;
        } else if (strcmp(weather.forecasts[i].icon, "rain") == 0) {
            forecast_image = &rain;
        } else if (strcmp(weather.forecasts[i].icon, "snow") == 0) {
            forecast_image = &snow;
        } else if (strcmp(weather.forecasts[i].icon, "sleet") == 0) {
            forecast_image = &sleet;
        } else if (strcmp(weather.forecasts[i].icon, "wind") == 0) {
            forecast_image = &strongwind;
        } else if (strcmp(weather.forecasts[i].icon, "fog") == 0) {
            forecast_image = &fog;
        } else if (strcmp(weather.forecasts[i].icon, "cloudy") == 0) {
            forecast_image = &cloudy;
        } else if (strcmp(weather.forecasts[i].icon, "partly-cloudy-day") == 0) {
            forecast_image = &daycloudy;
        } else if (strcmp(weather.forecasts[i].icon, "partly-cloudy-night") == 0) {
            forecast_image = &nightaltcloudy;
        }

        if (forecast_image != NULL) {
            draw_bitmap_mono_in_center(7, i, 400, 255, forecast_image);
        }
    }

    draw_string_in_grid_align_left(1, 0, 2, 400, 0, CONFIG_PLACE_NAME, &Ubuntu12);

    now = weather.observed_at;
    char strftime_buf[64];
    // Use the configured local timezone.
    setenv("TZ", CONFIG_DISPLAY_TIMEZONE, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "Data: %d/%m %H:%M", &timeinfo);

    draw_string_in_grid_align_right(1, 0, 2, 400, 0, strftime_buf, &Ubuntu12);

    draw_string_in_grid_align_left(1, 0, 4, 400, 180,
        "Weather: Open-Meteo.com", &Ubuntu8);

    draw_horizontal_line(0, 14, 400, COLORED);
    draw_horizontal_line(0, 200, 400, COLORED);
    draw_horizontal_line(0, 0, 400, COLORED);
    draw_vertical_line(0, 0, 300, COLORED);
    draw_horizontal_line(0, 299, 400, COLORED);
    draw_vertical_line(399, 0, 300, COLORED);

    for (size_t i = 1; i < 7; i++) {
        draw_vertical_line((400 / 7 * i), 200, 138, COLORED);
    }

    // /* Display the frame buffer */
    display_frame(NULL, frame_black);

    epd4in2_sleep();

    free(frame_black);
}

static void update_time_using_ntp(void)
{
    static const char* TAG = "update_time_using_ntp_task";

    time_t now;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (2016 - 1900)
    // Time updated in last 24 hours? If not, ((time_updated + 60 * 60 * 24) < now)
    if (timeinfo.tm_year < (2016 - 1900) || ((time_updated + 60 * 60 * 24) < now)) {
        ESP_LOGI(TAG, "Time is not set yet or time is not updated last 24h. Connecting to WiFi and getting time over NTP.");
        bool synchronized = obtain_time();
        time(&now);
        if (synchronized) time_updated = now;
    }

    char strftime_buf[64];
    // Use the configured local timezone.
    setenv("TZ", CONFIG_DISPLAY_TIMEZONE, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current local date/time is: %s", strftime_buf);

    esp_sntp_stop();
}

void app_main(void)
{
    static const char* TAG = "app_main";

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    int deep_sleep_sec = 3 * 60 * 60;

    if (!initialise_wifi()) {
        init_ota_button();

        if (check_if_ota_button_pressed()) {
            xTaskCreate(&ota_task, "ota_example_task", 1024 * 14, NULL, 5, NULL);
            vTaskDelay(1200000 / portTICK_PERIOD_MS);
            deinitialize_wifi();
        } else {

            // TLS needs a valid clock. Complete network work before stopping Wi-Fi.
            update_time_using_ntp();
            esp_err_t weather_err = get_current_weather();
            deinitialize_wifi();
            if (weather_err == ESP_OK) {
                weather_to_display();
            } else {
                ESP_LOGW(TAG, "Weather unavailable; keeping the previous e-paper image");
            }

            time_t now;
            struct tm timeinfo = {0};

            time(&now);
            setenv("TZ", CONFIG_DISPLAY_TIMEZONE, 1);
            tzset();
            localtime_r(&now, &timeinfo);

            int seconds_of_today_ahead = (timeinfo.tm_sec + (timeinfo.tm_min * 60) + (timeinfo.tm_hour * 60 * 60));

            bool sleep_time_set = false;

            for (size_t i = 0; i < (sizeof(update_times) / sizeof(update_times[0])); i++) {
                if (seconds_of_today_ahead < (update_times[i] * 60)) {
                    deep_sleep_sec = (update_times[i] * 60) - seconds_of_today_ahead;
                    sleep_time_set = true;
                    break;
                }
            }

            if (!sleep_time_set) {
                deep_sleep_sec = (24 * 60 * 60 - seconds_of_today_ahead) + (update_times[0] * 60);
            }
        }
    }

    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
