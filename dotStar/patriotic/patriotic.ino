#include <stdio.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>

#define NUMPIXELS 144

Adafruit_DotStar strip(NUMPIXELS, DOTSTAR_BRG);

const long SLIDE_DELAY = 4;
const long REPOSITION_DELAY = 500;

const long RED = 0x00FF00;
const long WHITE = 0xFFFFFF;
const long BLUE = 0x0000FF;

void renderBlock(long centerIndex) {
  long whiteIndex = centerIndex;
  long redIndex = centerIndex - 1;  
  long blueIndex = centerIndex + 1;

  strip.setPixelColor(redIndex, RED);
  strip.setPixelColor(whiteIndex, WHITE);
  strip.setPixelColor(blueIndex, BLUE);
  strip.show();
}

void clearBlock(const long index) {
    strip.setPixelColor(index - 1, 0);
    strip.setPixelColor(index, 0);
    strip.setPixelColor(index + 1, 0);
}

void slideTo(const long start, const long target) {
  if(target > start) {
    slideRight(start, target);
  } else {
    slideLeft(start, target);
  }
}

void slideLeft(long next, const long distance) {
  while(next > distance) {
    clearBlock(next);
    next = next - 1;
    renderBlock(next);
    delay(SLIDE_DELAY);
  }
}

void slideRight(long next, const long distance) {
  while(next < distance) {
    clearBlock(next);
    next = next + 1;
    renderBlock(next);
    delay(SLIDE_DELAY);
  }
}

void setup() {
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1);
  #endif

  randomSeed(analogRead(0));

  strip.begin();
  strip.clear();
  strip.show();
}

long currentCenter = 1;
long nextCenter = 142;

void loop() {
  nextCenter = random(NUMPIXELS);
  slideTo(currentCenter, nextCenter);
  currentCenter = nextCenter;
  delay(REPOSITION_DELAY);
}
