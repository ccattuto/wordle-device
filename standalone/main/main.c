/*
    Wordle Device for the ESP32C3 RBG development board

    Written in 2022 by Ciro Cattuto <ciro.cattuto@gmail.com>
    
    To the extent possible under law, the author(s) have dedicated all copyright
    and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "freertos/FreeRTOS.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "main.h"
#include "wifi.h"
#include "ledmatrix.h"
#include "twitter.h"
#include "wordle.h"

const char *TAG = "wordle";

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ledmatrix_init();

    ret = wifi_init();
	// couldn't connect or get an IP
	if (!ret)
		blink_red_forever();

    // get time via NTP
    ret = obtain_time();
    // couldn't get time
    if (ret)
		blink_red_forever();

    // open TLS connection to Twitter API endpoint
    twitter_api_init();

    // run application (this never returns)
    wordle();
}
