/*
    Worlde Device for the ESP32C3 RBG development board

    Written in 2022 by Ciro Cattuto <ciro.cattuto@gmail.com>
    
    To the extent possible under law, the author(s) have dedicated all copyright
    and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "main.h"
#include "twitter.h"
#include "ledmatrix.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "esp_crt_bundle.h"

// Twitter API v2 streaming endpoint
#define BEARER_TOKEN CONFIG_TWITTER_BEARER_TOKEN
#define WEB_SERVER "api.twitter.com"
#define WEB_PORT "443"
#define WEB_URL "https://api.twitter.com/2/tweets/search/stream"

// streaming API request
static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "Authorization: Bearer " BEARER_TOKEN "\r\n"
    "\r\n";

// tweet queue
#define STREAM_BUF_SIZE 1024
StreamBufferHandle_t stream_buf;

static void https_stream_task(void *pvParameters)
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

        do {
            len = sizeof(buf) - 1;
            bzero(buf, sizeof(buf));
            ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);

            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;

            if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                ret = 0;
                break;
            }

            if (ret < 0) {
                ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
                break;
            }

            if (ret == 0) {
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

        if (ret != 0) {
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

void twitter_api_init(void) {
    // create stream buffer
    stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, 1);
    if (stream_buf == NULL)
		blink_red_forever();

    // start HTTPS/TLS streaming connection to Twitter v2 API
    xTaskCreate(&https_stream_task, "https_stream_task", 8192, NULL, 5, NULL);
}