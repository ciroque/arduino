#include <Adafruit_DotStar.h>
#include <SPI.h>

#define NUMPIXELS 60 // 60 for bright white led strip, 144 for dotStar color 144

//Adafruit_DotStar strip(NUMPIXELS, DOTSTAR_BRG);

#define DATAPIN    4
#define CLOCKPIN   5

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);


void setup() {
  Serial.begin(9600);

#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1); // Enable 16 MHz on Trinket
#endif

  randomSeed(analogRead(0));

  Serial.println("about to configure strip...");

  strip.begin();
  strip.clear();
  strip.show();

  Serial.println("Strip configured...");
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
    delay(random(50, 1000));  // Crazy: 10, 250; Busy: 20, 500; Moderate: 50, 1000 Lazy: 100, 2000
  } else {
    flasher--;
    delay(random(40, 120));
  }
  long intensity = generateIntensity();
  Serial.println(intensity);
  strip.setPixelColor(index, intensity);
  strip.show();
}
