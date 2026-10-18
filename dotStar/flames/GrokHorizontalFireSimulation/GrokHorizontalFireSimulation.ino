#include <Adafruit_DotStar.h>
#include <SPI.h>

// ----- Config -----
#define NUM_LEDS  136
#define DATAPIN   4
#define CLOCKPIN  5

Adafruit_DotStar strip(NUM_LEDS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

byte heat[NUM_LEDS];

uint32_t fireColor(byte t) {
    if (t < 70) {
        return strip.Color(t * 3, 0, 0);                    // deep red
    } else if (t < 140) {
        return strip.Color(255, (t - 70) * 2, 0);           // red → orange
    } else {
        byte g = 140 + (t - 140) * 2;
        return strip.Color(255, min(255, g), t > 220 ? (t - 220) : 0);
    }
}

void setup() {
    strip.begin();
    strip.setBrightness(72);
    strip.show();

    randomSeed(analogRead(A0));

    // Initial ember bed
    for (int i = 0; i < NUM_LEDS; i++) {
        heat[i] = random(20, 60);
    }
}

void loop() {
    // 1. Cool + slight random drift
    for (int i = 0; i < NUM_LEDS; i++) {
        int cooldown = random(3, 18);
        heat[i] = (cooldown > heat[i]) ? 0 : heat[i] - cooldown;
    }

    // 2. Heat spread
    byte blended[NUM_LEDS];
    for (int i = 0; i < NUM_LEDS; i++) {
        int total = heat[i] * 3;
        int count = 3;

        if (i > 0)     { total += heat[i - 1]; count++; }
        if (i < NUM_LEDS - 1) { total += heat[i + 1]; count++; }

        blended[i] = total / count;
    }
    memcpy(heat, blended, NUM_LEDS);

    // 3. Sparks
    if (random(255) < 65) {
        int spark = random(NUM_LEDS);
        int strength = random(100, 230);

        heat[spark] = min(255, heat[spark] + strength);

        if (spark > 0)
            heat[spark - 1] = min(255, heat[spark - 1] + random(25, 85));
        if (spark < NUM_LEDS - 1)
            heat[spark + 1] = min(255, heat[spark + 1] + random(25, 85));
    }

    // 4. Base heat bias (hotter near the "bottom")
    for (int i = NUM_LEDS * 7 / 10; i < NUM_LEDS; i++) {
        if (heat[i] < 55) heat[i] = random(45, 75);
    }

    // 5. Render
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, fireColor(heat[i]));
    }

    strip.show();
    delay(random(30, 75));
}