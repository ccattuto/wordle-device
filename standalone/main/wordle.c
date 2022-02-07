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
#include "wordle.h"
#include "twitter.h"
#include "ledmatrix.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"

// buffer holding JSON for a single tweet
#define TWEET_BUF_LEN 512

static int check_wordle_line(char *p, char *p2, char *buf) {
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

static int process_tweet(char *s, char *buf) {
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

        ledmatrix_update(wordle_buf, wordle_len);
    }
}
