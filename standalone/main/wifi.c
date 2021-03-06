/*
    Wordle Device for the ESP32C3 RGB development board

    Written in 2022 by Ciro Cattuto <ciro.cattuto@gmail.com>
    based on Espressif System's examples at
    https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station
    https://github.com/espressif/esp-idf/tree/master/examples/protocols/sntp

    To the extent possible under law, the author(s) have dedicated all copyright
    and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "main.h"
#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_tls.h"

// Wi-Fi
#define ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define ESP_WIFI_HOSTNAME CONFIG_ESP_WIFI_HOSTNAME

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT 0x01
#define WIFI_FAIL_BIT 0x02

// number of retries while connecting to Wi-Fi
static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < 10)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retrying connection to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connection to the AP failed");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

int wifi_init(void)
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_err_t ret = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, ESP_WIFI_HOSTNAME);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "failed to set hostname: %d", ret);
    }

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
       number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
       happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to SSID: %s", ESP_WIFI_SSID);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s", ESP_WIFI_SSID);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    return (bits & WIFI_CONNECTED_BIT) ? 1 : 0;
}
