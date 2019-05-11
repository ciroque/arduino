#include <Adafruit_DotStar.h>
#include <SPI.h>

#define NUMPIXELS 144

Adafruit_DotStar strip(NUMPIXELS, DOTSTAR_BRG);

void setup() {
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1);
  #endif

  strip.begin();
  strip.show();
}

const int TAIL_LENGTH = 14;
const int LEFT = 0;
const int RIGHT = 1;
int head = 0, tail = head - TAIL_LENGTH;
uint32_t color = 0x00FF00;
int direction = RIGHT;

void loop() {
  strip.setPixelColor(head, color);
  strip.setPixelColor(tail, 0);
  strip.show();

  if(head >= NUMPIXELS) {
    tail = head + TAIL_LENGTH;
    direction = LEFT;
  } else if(head < 0) {
    tail = head - TAIL_LENGTH;
    direction = RIGHT;
  }

  if(direction == RIGHT) {
    head++;
    tail++;
  } else {
    head--;
    tail--;
  }

  delay(10);
}
