# Standalone Wordle device firmware

This is a standalone firmware, written in C, that depends only on the [ESP-IDF Development Framework](https://github.com/espressif/esp-idf). It is heavily based on two of the framework examples: [Wi-Fi Station](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station) and [mbedTLS](https://github.com/espressif/esp-idf/tree/master/examples/protocols/https_mbedtls).

## Operation

The application connects to a Wi-Fi access point using an SSID and password specified in the configuration menu. It then connectes to the [Twitter v2 Filtered Stream API](https://developer.twitter.com/en/docs/twitter-api/tweets/filtered-stream/introduction) via HTTPS, using a Bearer Token generated as described [here](https://developer.twitter.com/en/docs/authentication/oauth-2-0/bearer-tokens), also specified in the configuration menu. Creating a bearer token requires the Twitter App to be part of a [project](https://developer.twitter.com/en/docs/projects/overview).

Once the application connects to the Twitter streaming API, it sets up the rule for filtering Wordle tweets (which can be modified in the configuration menu), and it starts consuming tweets matching the rule. The applications inspects the text of every incoming tweet for a Wordle solution, looking for Unicode colored squares, parses it, and visualizes it on the 5x5 LED matrix (excluding 6-lines solutions).

## Building

The application conforms to the [ESP-IDF template project](https://github.com/espressif/esp-idf-template) and is built as described in the [quick reference](https://github.com/espressif/esp-idf#quick-reference). The bare minimum to configure and build the application is:

`idf.py menuconfig`

to set the Wi-Fi SSID, password and the Twitter bearer token under "Wordle Device Configuration".
Followed by:

`idf.py flash`

or

`ide.py flash monitor`

to see the debug console and the text of incoming tweets.



