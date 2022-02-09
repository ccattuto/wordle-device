# Standalone Wordle-device firmware

This is a standalone firmware, written in C, that depends only on the [ESP-IDF Development Framework](https://github.com/espressif/esp-idf). It is heavily based on some of the examples of the framework: [Wi-Fi Station](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station), [SNTP](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sntp), and [mbedTLS](https://github.com/espressif/esp-idf/tree/master/examples/protocols/https_mbedtls).

## Operation

The application connects to a Wi-Fi access point using an SSID and password specified in the configuration menu. It then connectes to the [Twitter v2 Filtered Stream API](https://developer.twitter.com/en/docs/twitter-api/tweets/filtered-stream/introduction) via HTTPS, using a Bearer Token generated as described [here](https://developer.twitter.com/en/docs/authentication/oauth-2-0/bearer-tokens), which is also specified in the configuration menu. Using the Twitter v2 APIs requires the Twitter App to be part of a [project](https://developer.twitter.com/en/docs/projects/overview). NOTICE: right now the application assumes that the rules to filter Wordle tweets are already set up.

Once the streaming API is set up, the applications inspects the text of every incoming tweet for a Wordle solution, looking for Unicode colored squares, parses it and visualizes it on the 5x5 LED matrix (excluding 6-lines solutions).

## Building

The application is built according to the [ESP-IDF template project](https://github.com/espressif/esp-idf-template) and built as described in the [quick reference](https://github.com/espressif/esp-idf#quick-reference).


