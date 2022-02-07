/*
    Worlde Device for the ESP32C3 RBG development board

    Written in 2022 by Ciro Cattuto <ciro.cattuto@gmail.com>
    
    To the extent possible under law, the author(s) have dedicated all copyright
    and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef __LED_MATRIX_H__
#define __LED_MATRIX_H__

#include "driver/gpio.h"
#include "led_strip.h"

#define CONFIG_BLINK_LED_RMT_CHANNEL 1
#define BLINK_GPIO 8
#define CONFIG_BLINK_PERIOD 500

void ledmatrix_init(void);
void ledmatrix_update(char *buf, int num_lines);
void blink_red_forever(void);

#endif /* __LED_MATRIX_H__ */
