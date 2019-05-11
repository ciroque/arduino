#include <Adafruit_DotStar.h>
#include <SPI.h>

#define NUMPIXELS 60

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

long generateIntensity() {
  long base = random(255);
  return (base << 16) + (base << 8) + base;
}

long nextFlasher() {
  return random(1, 7);
}

long index = random(NUMPIXELS);
long flasher = nextFlasher();

void loop() {
  Serial.println(flasher);
  if (flasher <= 0) {
    strip.setPixelColor(index, 0);
    strip.show();
    index = random(NUMPIXELS);
    flasher = nextFlasher();
    delay(random(10, 2000));
  } else {
    flasher--;
    delay(random(60, 120));
  }
  long intensity = generateIntensity();
  Serial.println(intensity);
  strip.setPixelColor(index, intensity);
  strip.show();
}
