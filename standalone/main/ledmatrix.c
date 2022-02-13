/*
    Wordle Device for the ESP32C3 RGB development board

    Written in 2022 by Ciro Cattuto <ciro.cattuto@gmail.com>

    To the extent possible under law, the author(s) have dedicated all copyright
    and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "ledmatrix.h"
#include "main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// LED matrix configuration
static led_strip_t *pStrip;

void ledmatrix_init(void)
{
    pStrip = led_strip_init(CONFIG_BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 25);
    pStrip->clear(pStrip, 50);
}

void ledmatrix_update(char *buf, int num_lines)
{
    int i, j;

    ESP_LOGI(TAG, "showing %d-lines Wordle: %s", num_lines, buf);

    for (i = 0; i < 5 * (5 - num_lines); i++)
        pStrip->set_pixel(pStrip, i, 0, 0, 0);

    for (j = 0; i < 25; i++, j++)
    {
        switch (buf[j])
        {
        case 'G':
            pStrip->set_pixel(pStrip, i, 0, 32, 0);
            break;

        case 'Y':
            pStrip->set_pixel(pStrip, i, 32, 32, 0);
            break;

        case 'B':
        case 'W':
        default:
            pStrip->set_pixel(pStrip, i, 2, 2, 2);
            break;
        }
    }

    pStrip->refresh(pStrip, 100);
}

void blink_red_forever(void)
{
    while (1)
    {
        pStrip->set_pixel(pStrip, 12, 32, 0, 0); // central LED
        pStrip->refresh(pStrip, 100);
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
        pStrip->clear(pStrip, 50);
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
