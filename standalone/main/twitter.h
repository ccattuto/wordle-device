/*
    Wordle Device for the ESP32C3 RGB development board

    Written in 2022 by Ciro Cattuto <ciro.cattuto@gmail.com>

    To the extent possible under law, the author(s) have dedicated all copyright
    and related and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef __TWITTER_H__
#define __TWITTER_H__

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

// tweet queue
extern StreamBufferHandle_t stream_buf;

void twitter_api_init(void);

#endif /* __TWITTER_H__ **/
