#include <Adafruit_DotStar.h>
#include <SPI.h>

// ----- Config -----
#define NUM_LEDS 144
#define DATAPIN  4
#define CLOCKPIN 5

// Try DOTSTAR_BGR, DOTSTAR_BRG, DOTSTAR_RGB, etc. if colors are wrong.
Adafruit_DotStar strip(NUM_LEDS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

byte heat[NUM_LEDS];

uint32_t fireColor(byte temperature) {
  byte t = temperature;
  byte r, g, b;

  if (t < 80) {
    // Deep red ember
    r = t * 2;
    g = 0;
    b = 0;
  } else if (t < 170) {
    // Red to orange
    r = 255;
    g = (t - 80) * 2;
    b = 0;
  } else {
    // Yellow-hot flare, not much blue/white
    r = 255;
    g = 180 + ((t - 170) / 2);
    b = 0;
  }

  return strip.Color(r, g, b);
}

void setup() {
  strip.begin();
  strip.setBrightness(72); // Keep moderate for tissue/batting
  strip.show();

  randomSeed(analogRead(A0));
}

void loop() {
  // 1. Cool every cell slightly
  for (int i = 0; i < NUM_LEDS; i++) {
    int cooldown = random(4, 22);
    heat[i] = (cooldown > heat[i]) ? 0 : heat[i] - cooldown;
  }

  // 2. Blend heat sideways to make glowing patches
  byte blended[NUM_LEDS];

  for (int i = 0; i < NUM_LEDS; i++) {
    int total = heat[i] * 2;
    int count = 2;

    if (i > 0) {
      total += heat[i - 1];
      count++;
    }

    if (i < NUM_LEDS - 1) {
      total += heat[i + 1];
      count++;
    }

    blended[i] = total / count;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    heat[i] = blended[i];
  }

  // 3. Random flare-ups across the altar
  if (random(255) < 120) {
    int spark = random(NUM_LEDS);
    heat[spark] = min(255, heat[spark] + random(90, 220));

    // Warm neighboring pixels too
    if (spark > 0) {
      heat[spark - 1] = min(255, heat[spark - 1] + random(30, 100));
    }

    if (spark < NUM_LEDS - 1) {
      heat[spark + 1] = min(255, heat[spark + 1] + random(30, 100));
    }
  }

  // 4. Keep a dim ember bed alive
  for (int i = 0; i < NUM_LEDS; i++) {
    if (heat[i] < 35) {
      heat[i] = random(20, 45);
    }
  }

  // 5. Render LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, fireColor(heat[i]));
  }

  strip.show();
  delay(random(35, 80));
}