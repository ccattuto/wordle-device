#include <Adafruit_NeoPixel.h>

#define LED_PIN 8
Adafruit_NeoPixel strip = Adafruit_NeoPixel(25, LED_PIN, NEO_GRB + NEO_KHZ800);

int pixel = 0;
int cmd;

void setup() {
  strip.begin();
  strip.setBrightness(50);
  strip.show();

  Serial.begin(115200);
}

void loop() {
  if (Serial.available() > 0) {
    cmd = Serial.read();

    if (cmd == 'B') {
      strip.setPixelColor(pixel++, strip.Color(10, 10, 10));
    } else if (cmd == 'G') {
      strip.setPixelColor(pixel++, strip.Color(0, 128, 0));
    } else if (cmd == 'Y') {
      strip.setPixelColor(pixel++, strip.Color(64, 60, 0));
    } else if (cmd == 'Z') {
      strip.clear();
      pixel = 0;
    }

    strip.show();

    if (pixel == 25)
      pixel = 0;
  }
}

