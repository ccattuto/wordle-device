# wordle-device

Pull [Wordle](https://www.powerlanguage.co.uk/wordle/) solutions from the Twitter API v2 and [displays them](https://twitter.com/ciro/status/1488259161066459142) on the 5x5 LED matrix of [this ESP32-C3 development board](https://www.cnx-software.com/2022/01/07/board-with-25-rgb-leds-is-offered-with-esp32-c3-or-esp32-pico-d4/).

![Wordle Device](wordle-device.jpg)

There are two implementations here:

- `wordle-device.ino` and `wordle.py`: use the Arduino IDE and Espressif ESP32-C3 core to upload this sketch to the board; configure the Twitter API keys and tokens in the Python code and run locally with the board connected, to pull the Tweets and send data via serial for display
- `standalone`: the separate [`README.md`](standalone/README.md) file describes how to configure, install and run this standalone code directly from the board (no local Python code required)
