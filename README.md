# wordle-device

Pull [Wordle](https://www.nytimes.com/games/wordle/index.html) solutions from the Twitter API v2 and [displays them](https://twitter.com/ciro/status/1488259161066459142) on the 5x5 LED matrix of [the ESP32 C3FH4 RGB development board](https://www.cnx-software.com/2022/01/07/board-with-25-rgb-leds-is-offered-with-esp32-c3-or-esp32-pico-d4/) by [01Space](https://github.com/01Space).

![Wordle Device](wordle-device.jpg)

There are two implementations in this repository:

- `wordle-device.ino` and `wordle.py`: use the Arduino IDE and Espressif ESP32-C3 core to upload this sketch to the board; configure the Twitter API keys and tokens in the Python code (marked by `CHANGEME`) and run locally with the board connected, to pull the Tweets and send data via serial for display.
  - Arduino requires the Adafruit NeoPixel library
  - Python code requires Tweepy to run (`pip3 install tweepy`)
- `standalone`: the separate [`README.md`](standalone/README.md) file describes how to configure, install and run this standalone code directly from the board (no local Python code required)

Watch a [video](https://www.youtube.com/watch?v=2UfY--8PEmA) of the standalone version.
