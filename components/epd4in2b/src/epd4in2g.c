/*
 * Adapted from Waveshare team's EPD_4in2g.cpp (2025-01-07).
 * https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/4in2_e-Paper_G/ESP32/EPD_4in2g.cpp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "epd4in2g.h"
#include "epdif.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char *TAG = "epd4in2g";

static void command(uint8_t value)
{
    digital_write(DC_PIN, 0);
    spi_transfer(value);
}

static void data(uint8_t value)
{
    digital_write(DC_PIN, 1);
    spi_transfer(value);
}

static esp_err_t wait_ready(const char *stage)
{
    const int64_t start = esp_timer_get_time();
    // Match the G demo's ReadBusyH() function, NOT its misleading comments
    // or unused ReadBusy(): wait while LOW, return when HIGH.
    while (digital_read(BUSY_PIN) == 0) {
        if (esp_timer_get_time() - start >= 90000000) {
            ESP_LOGE(TAG, "%s: BUSY GPIO%d remained LOW for 90s", stage, BUSY_PIN);
            return ESP_ERR_TIMEOUT;
        }
        delay_ms(5);
    }
    ESP_LOGI(TAG, "%s: ready HIGH after %lld ms", stage,
             (long long)((esp_timer_get_time() - start) / 1000));
    return ESP_OK;
}

esp_err_t epd4in2g_init(void)
{
    if (ifinit() != 0) return ESP_FAIL;
    ESP_LOGI(TAG, "Waveshare 4.2-inch (G), 400x300 black/white/yellow/red; BUSY LOW=busy HIGH=ready");
    digital_write(RST_PIN, 1);
    delay_ms(200);
    digital_write(RST_PIN, 0);
    esp_rom_delay_us(2000);
    digital_write(RST_PIN, 1);
    delay_ms(200);

    // The G demo sends configuration after reset, then powers on and waits.
    // 0x12 is DISPLAY_REFRESH here; never send the monochrome V2 SWRESET.
    command(0x4d); data(0x78);
    command(0x00); data(0x0f); data(0x29);
    command(0x06);
    data(0x0d); data(0x12); data(0x24); data(0x25);
    data(0x12); data(0x29); data(0x10);
    command(0x30); data(0x08);
    command(0x50); data(0x37);
    command(0x61);
    data(EPD_WIDTH >> 8); data(EPD_WIDTH & 0xff);
    data(EPD_HEIGHT >> 8); data(EPD_HEIGHT & 0xff);
    command(0xae); data(0xcf);
    command(0xb0); data(0x13);
    command(0xbd); data(0x07);
    command(0xbe); data(0xfe);
    command(0xe9); data(0x01);
    command(0x04); // POWER_ON
    delay_ms(100); // allow BUSY to assert before checking completion
    return wait_ready("power on");
}

// Spread four monochrome bits into four 2-bit black/white color codes.
static uint8_t expand_nibble(uint8_t bits)
{
    return ((bits & 0x08) << 3) | ((bits & 0x04) << 2)
         | ((bits & 0x02) << 1) | (bits & 0x01);
}

void epd4in2g_convert_mono(const uint8_t *mono, uint8_t *color)
{
    for (size_t i = 0; i < EPD_FRAME_BYTES; ++i) {
        color[2 * i] = expand_nibble(mono[i] >> 4);
        color[2 * i + 1] = expand_nibble(mono[i] & 0x0f);
    }
}

static esp_err_t refresh(void)
{
    ESP_LOGI(TAG, "Starting G four-color refresh; waiting for BUSY LOW then HIGH");
    command(0x12);
    data(0x00);
    const int64_t start = esp_timer_get_time();
    while (digital_read(BUSY_PIN) == 1) {
        if (esp_timer_get_time() - start >= 1000000) {
            ESP_LOGE(TAG, "No BUSY LOW after G refresh command; panel response not detected");
            return ESP_ERR_TIMEOUT;
        }
        delay_ms(1);
    }
    esp_err_t err = wait_ready("full refresh");
    if (err == ESP_OK) ESP_LOGI(TAG, "G full refresh BUSY cycle completed");
    return err;
}

esp_err_t epd4in2g_display_mono(const uint8_t *frame)
{
    if (frame == NULL) return ESP_ERR_INVALID_ARG;
    const int64_t transfer_start = esp_timer_get_time();
    command(0x10); // packed 2-bit pixels, 4 pixels per byte
    for (size_t i = 0; i < EPD_FRAME_BYTES; ++i) {
        data(expand_nibble(frame[i] >> 4));
        data(expand_nibble(frame[i] & 0x0f));
    }
    ESP_LOGI(TAG, "Image transfer: %d bytes in %lld ms", EPD_COLOR_FRAME_BYTES,
             (long long)((esp_timer_get_time() - transfer_start) / 1000));
    return refresh();
}

esp_err_t epd4in2g_display_color(const uint8_t *frame)
{
    if (frame == NULL) return ESP_ERR_INVALID_ARG;
    const int64_t transfer_start = esp_timer_get_time();
    command(0x10);
    for (size_t i = 0; i < EPD_COLOR_FRAME_BYTES; ++i) data(frame[i]);
    ESP_LOGI(TAG, "Image transfer: %d bytes in %lld ms", EPD_COLOR_FRAME_BYTES,
             (long long)((esp_timer_get_time() - transfer_start) / 1000));
    return refresh();
}

esp_err_t epd4in2g_sleep(void)
{
    command(0x02); // POWER_OFF
    data(0x00);
    delay_ms(100);
    esp_err_t err = wait_ready("power off");
    if (err != ESP_OK) return err;
    command(0x07); // DEEP_SLEEP
    data(0xa5);
    return ESP_OK;
}
