#!/usr/bin/env python
import sys
from serial.tools import list_ports
import serial
import tweepy

# locate ESP32-C3 USB device
port = None
for device in list_ports.comports():
    if device.vid == 0x303a and device.pid == 0x1001:
        break

if not device:
    sys.exit(-1)

ser = serial.Serial(device.device, baudrate=115200)

# Twitter streaming API

# CHANGE ME - your consumer key/secret below:
CONSUMER_KEY = 'XXX'
CONSUMER_SECRET = 'XXX'

# CHANGE ME - your access token/secret below:
ACCESS_TOKEN = 'XXX'
ACCESS_TOKEN_SECRET = 'XXX'

# LED matrix control (implemented in wordle.ino):
# - 5x5 matrix is viewed as a LED strip
# - sending 'Z' clears matrix and position cursor on first pixel (0)
# - sending 'G' / 'Y' / 'B' writes a green / yellow / "dark gray" pixel and advances cursor

# clear LED matrix
ser.write('Z'.encode())

# maps characters in tweet to 1-char commands over serial
symbol_map = {
    '🟩': 'G',
    '🟨': 'Y',
    '⬛': 'B',
    '⬜': 'B'
}

# write Wordle rows to LED matrix
def display_wordle(rows):
    cmd = "Z"
    for row in rows:
        cmd += "".join([symbol_map[s] for s in row])
    ser.write(cmd.encode())

# check whether a row of text is a worlde row
def is_worlde_row(s):
    if len(s) != 5:
        return False
    for c in s:
        if not c in symbol_map:
            return False
    return True;

# looks for 1 to 5 consecutive "worlde rows" in tweet
# and pack them into a list. Returns [] otherwise.
def extract_wordle(text):
    wordle = []
    in_wordle = False
    for row in text.split("\n"):
        if (in_wordle):
            if not is_worlde_row(row):
                break
            wordle.append(row)
        else:
            if is_worlde_row(row):
                in_wordle = True
                wordle.append(row)

    # sorry, we don't have space for solutions with 6 rows
    if (len(wordle) == 0 or len(wordle) > 5):
        return []
    
    # we require the last line to be the solution
    if (wordle[-1] != u'🟩🟩🟩🟩🟩'):
        return []

    return wordle


# process tweet
def process_tweet(text):
    wordle = extract_wordle(text)
    if len(wordle) == 0:
        return

    # if we've found a wordle, print it and display it on the LED matrix
    print (text)
    display_wordle(wordle)

# subclass tweepy
class WordleStream(tweepy.Stream):
    def on_status(self, status):
        process_tweet(status.text)

wordle_stream = WordleStream(CONSUMER_KEY, CONSUMER_SECRET, ACCESS_TOKEN, ACCESS_TOKEN_SECRET)

# filter tweets containing the keyword 'worlde'
wordle_stream.filter(track=["wordle"])
