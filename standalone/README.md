# Standalone Wordle device firmware

This is a standalone application, written in C, that depends only on the [ESP-IDF Development Framework](https://github.com/espressif/esp-idf). It is heavily based on two of the framework examples, [Wi-Fi Station](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station) and [mbedTLS](https://github.com/espressif/esp-idf/tree/master/examples/protocols/https_mbedtls), and relies on [LwJSON](https://github.com/MaJerle/lwjson) for parsing JSON.

## Operation

The application connects to a Wi-Fi access point using an SSID and password specified in the configuration menu. It then connects to the [Twitter v2 Filtered Stream API](https://developer.twitter.com/en/docs/twitter-api/tweets/filtered-stream/introduction) via HTTPS, using a Bearer Token generated as described [here](https://developer.twitter.com/en/docs/authentication/oauth-2-0/bearer-tokens), also specified in the configuration menu. Access to Twitter API v2 with a bearer token requires the Twitter App to be part of a [project](https://developer.twitter.com/en/docs/projects/overview).

The application assumes that a rule to filter Tweets to be visualized has already been configured, and that the rule is tagged as `wordle`. The rule's tag can be changed in the configuration menu. To add such a rule, you can proceed as described in [Twitter's filtered stream quick start guide](https://developer.twitter.com/en/docs/twitter-api/tweets/filtered-stream/quick-start) and add a rule as follows, replacing `$APP_ACCESS_TOKEN` with your Bearer Token:

```shell
curl -X POST 'https://api.twitter.com/2/tweets/search/stream/rules' \
-H "Content-type: application/json" \
-H "Authorization: Bearer $APP_ACCESS_TOKEN" -d \
'{
  "add": [
    {"value": "wordle OR #wordle", "tag": "wordle"}
  ]
}'
```

The filter query could be modified to include or exclude Tweets by modifying the `value` in the rule, following the Twitter [query syntax](https://developer.twitter.com/en/docs/twitter-api/tweets/filtered-stream/integrate/build-a-rule#build).

After connecting to  the Twitter streaming API, the application starts consuming incoming Tweets that match the above `wordle` filtering rule. The application inspects the text of every incoming Tweet for a Wordle solution, looking for Unicode colored squares, then parses it, and visualizes it on the 5x5 LED matrix (excluding 6-lines solutions).

## Building

The application conforms to the [ESP-IDF template project](https://github.com/espressif/esp-idf-template) and is built as described in the [ESP-IDF quick reference](https://github.com/espressif/esp-idf#quick-reference). The bare minimum required to configure and build the application is:

`idf.py menuconfig`

to set the Wi-Fi SSID, password, hostname, and the Twitter bearer token under "Wordle Device Configuration".

Followed by:

`idf.py flash`

or

`idf.py flash monitor`

to see the debug console and the text of incoming Tweets.
