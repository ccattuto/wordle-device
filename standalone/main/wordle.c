/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/stream_buffer.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "time.h"
#include "sys/time.h"
#include "esp_sntp.h"
#include "esp_tls.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "esp_crt_bundle.h"

#include "driver/gpio.h"
#include "led_strip.h"

// Wi-Fi 
#define ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD

// LED matrix configuration
static led_strip_t *pStrip;
#define CONFIG_BLINK_LED_RMT_CHANNEL 1
#define BLINK_GPIO 8
#define CONFIG_BLINK_PERIOD 500

// Twitter API v2 streaming endpoint
#define BEARER_TOKEN CONFIG_TWITTER_BEARER_TOKEN
#define WEB_SERVER "api.twitter.com"
#define WEB_PORT "443"
#define WEB_URL "https://api.twitter.com/2/tweets/search/stream"

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "Authorization: Bearer " BEARER_TOKEN "\r\n"
    "\r\n";

static const char *TAG = "wordle";

// tweet queue
#define STREAM_BUF_SIZE 1024
StreamBufferHandle_t stream_buf;

// buffer holding JSON for a single tweet
#define TWEET_BUF_LEN 512

void https_stream_task(void *pvParameters)
{
    char buf[512];
    int ret, flags, len;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ESP_LOGI(TAG, "Seeding the random number generator");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        abort();
    }

    ESP_LOGI(TAG, "Attaching the certificate bundle...");

    ret = esp_crt_bundle_attach(&conf);

    if(ret < 0)
    {
        ESP_LOGE(TAG, "esp_crt_bundle_attach returned -0x%x\n\n", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting hostname for TLS session...");

     /* Hostname set here should match CN in server certificate */
    if((ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");

    if((ret = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        goto exit;
    }

    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.

       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&conf, CONFIG_MBEDTLS_DEBUG_LEVEL);
#endif

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        goto exit;
    }

    while(1) {
        mbedtls_net_init(&server_fd);

        ESP_LOGI(TAG, "Connecting to %s:%s...", WEB_SERVER, WEB_PORT);

        if ((ret = mbedtls_net_connect(&server_fd, WEB_SERVER,
                                      WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
        {
            ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
            goto exit;
        }

        ESP_LOGI(TAG, "Connected.");

        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
                goto exit;
            }
        }

        ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

        if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
        {
            /* In real life, we probably want to close connection if ret != 0 */
            ESP_LOGW(TAG, "Failed to verify peer certificate!");
            bzero(buf, sizeof(buf));
            mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
            ESP_LOGW(TAG, "verification info: %s", buf);
        }
        else {
            ESP_LOGI(TAG, "Certificate verified.");
        }

        ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(&ssl));

        ESP_LOGI(TAG, "Writing HTTP request...");

        size_t written_bytes = 0;
        do {
            ret = mbedtls_ssl_write(&ssl,
                                    (const unsigned char *)REQUEST + written_bytes,
                                    strlen(REQUEST) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_WANT_READ) {
                ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
                goto exit;
            }
        } while(written_bytes < strlen(REQUEST));

        ESP_LOGI(TAG, "Reading HTTP response...");

        do
        {
            len = sizeof(buf) - 1;
            bzero(buf, sizeof(buf));
            ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);

            if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;

            if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                ret = 0;
                break;
            }

            if(ret < 0)
            {
                ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
                break;
            }

            if(ret == 0)
            {
                ESP_LOGI(TAG, "connection closed");
                break;
            }

            len = ret;
           
            // enqueue tweet
            ret = xStreamBufferSend(stream_buf, buf, len, pdMS_TO_TICKS(1000));
            ESP_LOGD(TAG, "%d bytes read, %d bytes enqueued", len, ret);
        } while(1);

        mbedtls_ssl_close_notify(&ssl);

    exit:
        mbedtls_ssl_session_reset(&ssl);
        mbedtls_net_free(&server_fd);

        if(ret != 0)
        {
            mbedtls_strerror(ret, buf, 100);
            ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
        }

        putchar('\n'); // JSON output doesn't have a newline at end

        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);
        printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Restarting Twitter API HTTPS/TSL connection...");
    }
}

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT  0x01
#define WIFI_FAIL_BIT       0x02

// number of retries while connecting to Wi-Fi
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

int wifi_init_sta(void)
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to SSID:%s password:%s", ESP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", ESP_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

	return (bits & WIFI_CONNECTED_BIT) ? 1 : 0;
}

void blink_red_forever(void) {
	while (1) {
       	pStrip->set_pixel(pStrip, 0, 32, 0, 0);
       	pStrip->refresh(pStrip, 100);
       	vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
       	pStrip->clear(pStrip, 50);
       	vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
	}
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_setservername(0, "pool.ntp.org");

    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    sntp_init();

    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (sntp_getservername(i))
            ESP_LOGI(TAG, "server %d: %s", i, sntp_getservername(i));
    }
}

int obtain_time(void) {
    char strftime_buf[64];

    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 50;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if (retry == retry_count)
        return -1;

    time(&now);
    localtime_r(&now, &timeinfo);

    // Set timezone to CET
    setenv("TZ", "UTC", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current UTC date/time is: %s", strftime_buf);

    return 0;
}

void display_update(char *buf, int num_lines) {
    int i, j;

    ESP_LOGI(TAG, "showing %d-lines Wordle: %s", num_lines, buf);

    for (i=0 ; i < 5*(5-num_lines); i++)
        pStrip->set_pixel(pStrip, i, 0, 0, 0);

    for (j=0 ; i < 25; i++, j++) {
        switch (buf[j]) {
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

int check_wordle_line(char *p, char *p2, char *buf) {
	uint8_t *s = (uint8_t *) p;
	int len = p2 - p;
	int i = 0;
	int count = 0;

	while (i < len) {
		if (s[i] == 0xF0) { // green or yellow
			if (i+4 > len)
				return 0;
			if (s[i+1] != 0x9F || s[i+2] != 0x9F)
				return 0;
			if (s[i+3] == 0xA9) { // green
				if (count < 5)
					buf[count] = 'G';
				count++;
			} else if (s[i+3] == 0xA8) { // yellow
				if (count < 5)
					buf[count] = 'Y';
				count++;
			} else
				return 0;
			i += 4;

		} else if (s[i] == 0xE2) { // black or white
			if (i+3 > len)
				return 0;
			if (s[i+1] != 0xAC)
				return 0;
			if (s[i+2] == 0x9B) { // black
				if (count < 5)
					buf[count] = 'B';
				count++;
			} else if (s[i+2] == 0x9C) { // white
				if (count < 5)
					buf[count] = 'W';
				count++;
			} else
				return 0;
			i += 3;

		} else
			break;
	}

	if (count == 5)
		return 1;
	else
		return 0;
}

int process_tweet(char *s, char *buf) {
	int len = strlen(s);
	char *p1, *p2;
	int wordle_lines = 0;

	p1 = s;
	while (p1 < s+len) {
		p2 = strstr(p1, "\\n");
		if (p2 == NULL)
			p2 = s+len;
		if (check_wordle_line(p1, p2, buf)) {
			wordle_lines += 1;
			if (wordle_lines > 6)
				break;
			buf += 5;
		} else if (wordle_lines > 0)
			break;

		p1 = p2 + 2;
	}

	return wordle_lines;
}

void wordle(void) {
//void wordle_task(void *pvParameters) {
    char tweet_buf[TWEET_BUF_LEN];
    char wordle_buf[5*6 + 1] = {0, };
    int len;
    int wordle_len;
    char *pos1, *pos2;
    int i;

    while (1) {
        len = xStreamBufferReceive(stream_buf, tweet_buf, TWEET_BUF_LEN, portMAX_DELAY);

        if (tweet_buf[0] != '{' || tweet_buf[len-1] != '\n')
            continue;

        ESP_LOGI(TAG, "got tweet");

        pos1 = strstr(tweet_buf, "\"text\":\"");
        if (pos1 == NULL)
            continue;
        pos1 += 8;
        
        pos2 = strchr(pos1, '"');
        if (pos2 == NULL)
            continue;

        *pos2 = 0;
        printf("%s\r\n", pos1);

        wordle_len = process_tweet(pos1, wordle_buf);

        if (wordle_len == 0 || wordle_len > 5)
			continue;

		for (i=0; i<5; i++) {
			if (wordle_buf[5*(wordle_len-1) + i] != 'G')
				break;
        }
		if (i < 5)
			continue;

		wordle_buf[5*wordle_len] = 0;
        //printf("%s\r\n", wordle_buf);

        display_update(wordle_buf, wordle_len);
    }
}

void app_main(void)
{
    pStrip = led_strip_init(CONFIG_BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 25);
    pStrip->clear(pStrip, 50);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    ret = wifi_init_sta();

	// couldn't connect or get an IP
	if (!ret)
		blink_red_forever();

    // get time via NTP
    ret = obtain_time();

    // couldn't get time
    if (ret)
		blink_red_forever();

    // create stream buffer
    stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, 1);
    if (stream_buf == NULL)
		blink_red_forever();

    // start HTTPS/TLS streaming connection to Twitter v2 API
    xTaskCreate(&https_stream_task, "https_stream_task", 8192, NULL, 5, NULL);

    wordle();
}
