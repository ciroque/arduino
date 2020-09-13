#include <stdio.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>

#define NUMPIXELS 144
#define OFF 0
#define RED 0x00FF00
#define WHITE 0xFFFFFF
#define BLUE 0x0000FF
#define COLOR_COUNT 3

//Adafruit_DotStar strip(NUMPIXELS, DOTSTAR_BRG);

#define DATAPIN    4
#define CLOCKPIN   5

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

void setup() {
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1);
  #endif

  randomSeed(analogRead(0));

  strip.begin();
  strip.clear();
  strip.show();
}

int index = random(NUMPIXELS);
const long colors[] = { RED, WHITE, BLUE };

long getNextColor() {
  return colors[random(COLOR_COUNT)];
}

void loop() {
  strip.setPixelColor(index, OFF);
  index = random(NUMPIXELS);
  strip.setPixelColor(index, getNextColor());
  strip.show();
  delay(10);
}
