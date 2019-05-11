#include <Adafruit_DotStar.h>
#include <SPI.h>

#define NUMPIXELS 144

Adafruit_DotStar strip(NUMPIXELS, DOTSTAR_BRG);

void setup() {
  Serial.begin(9600);

#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1); // Enable 16 MHz on Trinket
#endif

  randomSeed(analogRead(0));

  strip.begin();
  strip.show();
}

long generateColor() {
  long r = random(255);
  long g = random(255);
  long b = random(255);

  return (b << 16) + (r << 8) + g;
}

const long OFF = 0x000000;
long index = random(NUMPIXELS);

void loop() {
  strip.setPixelColor(index, OFF);
  index = random(NUMPIXELS);
  strip.setPixelColor(index, generateColor());
  strip.show();
  delay(6);
}
