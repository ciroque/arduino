#include <Adafruit_DotStar.h>
#include <SPI.h>

// --------- CONFIG ----------
#define NUMPIXELS 60
#define DATAPIN   4
#define CLOCKPIN  5

// Storm profile (tweak these)
#define STRIKE_GAP_MIN_MS   200   // time between strikes
#define STRIKE_GAP_MAX_MS  3000

#define STROKE_GAP_MIN_MS    10   // time between return strokes within a strike
#define STROKE_GAP_MAX_MS    90

#define DECAY_STEP_MIN_MS     2   // per-frame decay timing
#define DECAY_STEP_MAX_MS    12

#define SEGMENT_RADIUS_MIN    0   // 0 => single pixel, >0 => area flash
#define SEGMENT_RADIUS_MAX    4   // max +/- pixels around index (4 => up to 9 pixels)

#define SKY_FLASH_CHANCE      28  // 1/N chance per strike to do a whole-strip low flash (e.g. 28 => ~3.5%)
#define DEBUG_SERIAL          0

// Order may vary by strip; you had DOTSTAR_BRG so keep it
Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

// --------- HELPERS ----------
static inline uint32_t gray(uint8_t v) {
  // Pack grayscale into 0xRRGGBB
  return ((uint32_t)v << 16) | ((uint32_t)v << 8) | (uint32_t)v;
}

static inline uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

// Bias toward bright values (lightning peaks near white).
static uint8_t brightLevel(uint8_t minV, uint8_t maxV) {
  // Quadratic bias: pick two uniforms and take the max.
  uint8_t a = (uint8_t)random(minV, (int)maxV + 1);
  uint8_t b = (uint8_t)random(minV, (int)maxV + 1);
  return (a > b) ? a : b;
}

static void setSegment(int center, int radius, uint8_t level) {
  // Simple linear falloff around center.
  for (int d = -radius; d <= radius; d++) {
    int i = center + d;
    if (i < 0 || i >= NUMPIXELS) continue;

    int falloff = radius == 0 ? 0 : (abs(d) * 255) / radius; // 0..255
    uint8_t v = clampU8((int)level - (falloff / 3));        // gentler falloff
    strip.setPixelColor(i, gray(v));
  }
}

static void clearSegment(int center, int radius) {
  for (int d = -radius; d <= radius; d++) {
    int i = center + d;
    if (i < 0 || i >= NUMPIXELS) continue;
    strip.setPixelColor(i, 0);
  }
}

// Whole-strip "sky flash" (low intensity, short)
static void skyFlash(uint8_t level, int holdMs) {
  for (int i = 0; i < NUMPIXELS; i++) strip.setPixelColor(i, gray(level));
  strip.show();
  delay(holdMs);
  strip.clear();
  strip.show();
}

// One return stroke: fast bright hit + quick decay.
// Keeps changes localized to a segment for realism.
static void stroke(int center, int radius, uint8_t peak, uint8_t tail) {
  // Bright hit (very fast)
  setSegment(center, radius, peak);
  strip.show();
  delay(random(1, 6)); // very quick rise/hold

  // Quick decay over a few frames
  // Levels: peak -> ... -> tail -> off
  const int steps = 5 + random(0, 4); // 5..8 steps
  for (int s = 1; s <= steps; s++) {
    // Linear-ish decay
    int v = peak - ((peak - tail) * s) / steps;
    setSegment(center, radius, (uint8_t)v);
    strip.show();
    delay(random(DECAY_STEP_MIN_MS, DECAY_STEP_MAX_MS + 1));
  }

  // Fade to off
  clearSegment(center, radius);
  strip.show();
}

// --------- STATE ----------
static int centerIndex = 0;
static int radius = 0;

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
#endif

#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1);
#endif

  randomSeed(analogRead(0));

  strip.begin();
  strip.clear();
  strip.show();

  // Initialize after seeding RNG
  centerIndex = random(NUMPIXELS);
  radius = random(SEGMENT_RADIUS_MIN, SEGMENT_RADIUS_MAX + 1);
}

void loop() {
  // Pick a strike location / size
  centerIndex = random(NUMPIXELS);
  radius = random(SEGMENT_RADIUS_MIN, SEGMENT_RADIUS_MAX + 1);

#if DEBUG_SERIAL
  Serial.print("strike @ ");
  Serial.print(centerIndex);
  Serial.print(" radius=");
  Serial.println(radius);
#endif

  // Occasionally do a whole-strip "sky flash" first
  if (random(SKY_FLASH_CHANCE) == 0) {
    uint8_t sky = (uint8_t)random(10, 40);  // low illumination
    skyFlash(sky, random(10, 35));
  }

  // 1–3 clustered return strokes
  int strokes = 1 + random(0, 3); // 1..3
  uint8_t firstPeak = brightLevel(220, 255);

  for (int k = 0; k < strokes; k++) {
    // Subsequent strokes a bit weaker
    uint8_t peak = (k == 0) ? firstPeak : (uint8_t)clampU8((int)firstPeak - random(20, 80));
    uint8_t tail = (uint8_t)clampU8((int)peak - random(120, 170)); // tail dimmer

    stroke(centerIndex, radius, peak, tail);

    // Gap between strokes inside the same strike
    if (k < strokes - 1) {
      delay(random(STROKE_GAP_MIN_MS, STROKE_GAP_MAX_MS + 1));
    }
  }

  // Pause between strikes
  delay(random(STRIKE_GAP_MIN_MS, STRIKE_GAP_MAX_MS + 1));
}
