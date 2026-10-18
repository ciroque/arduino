#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <math.h>

// ============================================================================
// HARDWARE PIN ASSIGNMENTS
// ============================================================================
//
// NeoPixel data line:
//   Nano Every D4 -> NeoPixel DIN
//
// Momentary buttons:
//   D7  -> trigger button -> GND
//   D10 -> portal color button -> GND
//   D11 -> discharge direction button -> GND
//   D12 -> acceleration curve button -> GND
//
// All buttons use INPUT_PULLUP, so:
//
//   not pressed = HIGH
//   pressed     = LOW
//
// No external pull-up resistors are required.
//
// ============================================================================

#define DATA_PIN             4
#define TRIGGER_PIN          7
#define COLOR_BUTTON_PIN     10
#define DIRECTION_BUTTON_PIN 11
#define CURVE_BUTTON_PIN     12

// This Adafruit ring contains 12 NeoPixels.
#define NUM_PIXELS           12

// Create the NeoPixel ring object.
//
// NEO_GRB:
//   The ring expects bytes in Green/Red/Blue order.
//
// NEO_KHZ800:
//   Standard NeoPixel 800 kHz protocol.
Adafruit_NeoPixel ring(
  NUM_PIXELS,
  DATA_PIN,
  NEO_GRB + NEO_KHZ800
);

// ============================================================================
// PORTAL COLOR MODES
// ============================================================================
//
// BLUE_PORTAL:
//
//   idle pulse:
//     purple <-> blue
//
// ORANGE_PORTAL:
//
//   idle pulse:
//     orange <-> yellow
//
// ============================================================================

enum PortalColor {
  BLUE_PORTAL,
  ORANGE_PORTAL
};

// ============================================================================
// ANIMATION STATE MACHINE
// ============================================================================
//
// The entire effect is non-blocking except for the short curve-selection
// indicator blink.
//
// STATE_IDLE:
//
//   The ring slowly pulses between two colors.
//
// STATE_SPINUP:
//
//   A bright "energy head" rotates around the ring.
//   The rotation becomes progressively faster.
//
// STATE_BLAST:
//
//   All 12 LEDs go full white at maximum brightness.
//
// STATE_DISCHARGE:
//
//   A fast-moving colored energy effect rotates around the ring.
//   Its direction is configurable.
//
// STATE_DECAY:
//
//   The portal color fades down before returning to idle.
//
// ============================================================================

enum AnimationState {
  STATE_IDLE,
  STATE_SPINUP,
  STATE_BLAST,
  STATE_DISCHARGE,
  STATE_DECAY
};

// ============================================================================
// ACCELERATION CURVES
// ============================================================================
//
// These control how the frame delay changes during spin-up.
//
// Lower frame delay = faster rotation.
//
// CURVE_LINEAR:
//
//   Speed changes evenly throughout the spin-up.
//
// CURVE_QUADRATIC:
//
//   Stays slow longer, then accelerates harder.
//
// CURVE_CUBIC:
//
//   Even more pronounced late acceleration.
//
// CURVE_EXPONENTIAL:
//
//   Very slow early, then ramps very aggressively near the end.
//
// CURVE_SMOOTHSTEP:
//
//   Smooth acceleration with eased beginning and ending.
//
// CURVE_COUNT:
//
//   Not itself a curve.
//   Used to wrap the selection back to the first curve.
//
// ============================================================================

enum AccelerationCurve {
  CURVE_LINEAR,
  CURVE_QUADRATIC,
  CURVE_CUBIC,
  CURVE_EXPONENTIAL,
  CURVE_SMOOTHSTEP,

  CURVE_COUNT
};

// ============================================================================
// SIMPLE RGB COLOR STRUCTURE
// ============================================================================
//
// This avoids repeatedly passing three separate values around.
//
// Example:
//
//   RGB purple = {140, 0, 255};
//
// ============================================================================

struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// ============================================================================
// EEPROM CONFIGURATION
// ============================================================================
//
// The following three user-selectable settings survive a power cycle:
//
//   1. Portal color
//   2. Acceleration curve
//   3. Discharge direction
//
// They are written to the Nano Every's EEPROM.
//
// We use:
//
//   - a magic number
//   - a version number
//
// so we can determine whether EEPROM contains valid settings.
//
// ============================================================================

// ASCII-ish value representing "PORT".
//
// If EEPROM does not contain this value, we assume that the stored data is
// uninitialized or belongs to some unrelated/older program.
const uint32_t CONFIG_MAGIC = 0x504F5254;

// Increment this if the Config structure changes incompatibly.
//
// Example:
//
//   CONFIG_VERSION = 1
//
// Later:
//
//   CONFIG_VERSION = 2
//
// Old EEPROM data will then be rejected automatically.
const uint8_t CONFIG_VERSION = 1;

// Structure physically stored in EEPROM.
struct Config {

  // Identifies this EEPROM block as belonging to this program.
  uint32_t magic;

  // Allows future config structure changes.
  uint8_t version;

  // Stored enum value for BLUE_PORTAL / ORANGE_PORTAL.
  uint8_t portalColor;

  // Stored enum value for the acceleration curve.
  uint8_t accelerationCurve;

  // Either:
  //
  //   +1
  //   -1
  //
  // controls discharge rotation direction.
  int8_t dischargeDirection;
};

// Runtime copy of the EEPROM config.
Config config;

// ============================================================================
// CURRENT RUNTIME STATE
// ============================================================================

// Default portal color if EEPROM is empty or invalid.
PortalColor portalColor = BLUE_PORTAL;

// The program begins in idle mode.
AnimationState animationState = STATE_IDLE;

// Default spin-up acceleration curve.
AccelerationCurve accelerationCurve = CURVE_LINEAR;

// Discharge/unwind direction.
//
//   +1 = one direction
//   -1 = opposite direction
//
// The physical interpretation of "clockwise" depends on how the ring is
// mounted and from which side you are looking at it.
int dischargeDirection = -1;

// ============================================================================
// ACTIVE PORTAL COLOR PALETTE
// ============================================================================
//
// These values are populated by setPortalPalette().
//
// idleColor1 / idleColor2:
//
//   Endpoints used by the slow idle pulse.
//
// mediumColor:
//
//   Mid-intensity energy trail.
//
// hotColor:
//
//   Bright energy color.
//
// flashColor:
//
//   Very bright colored portion of discharge animation.
//
// ============================================================================

RGB idleColor1;
RGB idleColor2;
RGB mediumColor;
RGB hotColor;
RGB flashColor;

// ============================================================================
// BUTTON STATE / SOFTWARE DEBOUNCE
// ============================================================================
//
// Physical pushbuttons bounce electrically:
//
//   press
//   LOW
//   HIGH
//   LOW
//   HIGH
//   LOW
//
// ...within a few milliseconds.
//
// Without debouncing, one physical press could appear to be several presses.
//
// The code below waits for an input to remain stable for DEBOUNCE_MS before
// considering it a real state change.
//
// ============================================================================

// Trigger button state.
bool lastTriggerReading = HIGH;
bool triggerState = HIGH;

// Portal color button state.
bool lastColorReading = HIGH;
bool colorButtonState = HIGH;

// Discharge-direction button state.
bool lastDirectionReading = HIGH;
bool directionButtonState = HIGH;

// Acceleration curve button state.
bool lastCurveReading = HIGH;
bool curveButtonState = HIGH;

// Time when each input last changed electrically.
unsigned long triggerDebounceTime = 0;
unsigned long colorDebounceTime = 0;
unsigned long directionDebounceTime = 0;
unsigned long curveDebounceTime = 0;

// Number of milliseconds the input must remain stable.
const unsigned long DEBOUNCE_MS = 30;

// ============================================================================
// ACCELERATION CURVE INDICATOR
// ============================================================================
//
// When D12 changes the acceleration curve:
//
//   pixel 0 blinks green.
//
// Blink count is the 1-based curve index:
//
//   1 blink  = Linear
//   2 blinks = Quadratic
//   3 blinks = Cubic
//   4 blinks = Exponential
//   5 blinks = Smoothstep
//
// ============================================================================

// NeoPixel index to use as indicator.
const int CURVE_INDICATOR_PIXEL = 0;

// Blink timing.
const unsigned long CURVE_BLINK_ON_MS  = 120;
const unsigned long CURVE_BLINK_OFF_MS = 120;

// Full green indicator color.
const RGB CURVE_INDICATOR_COLOR = {
  0,
  255,
  0
};

// ============================================================================
// GENERAL ANIMATION TIMING
// ============================================================================

// Time the most recent animation frame was rendered.
unsigned long lastFrameTime = 0;

// Used for states that need to measure how long they have been active.
// Currently used by the full-white blast.
unsigned long stateStartTime = 0;

// ============================================================================
// IDLE ANIMATION SETTINGS
// ============================================================================
//
// The idle effect interpolates:
//
//   idleColor1 -> idleColor2 -> idleColor1
//
// IDLE_STEPS controls how many color increments are used.
//
// IDLE_FRAME_MS controls how long each increment lasts.
//
// Current timing:
//
//   100 steps * 20 ms = 2 seconds one direction
//
// therefore:
//
//   color 1 -> color 2 -> color 1
//
// takes about 4 seconds.
//
// ============================================================================

int idleStep = 0;

// +1 means moving toward idleColor2.
// -1 means moving back toward idleColor1.
int idleDirection = 1;

const int IDLE_STEPS = 100;
const unsigned long IDLE_FRAME_MS = 20;

// ============================================================================
// SPIN-UP SETTINGS
// ============================================================================
//
// SPIN_STEPS:
//
//   Number of animation frames during spin-up.
//
// More steps:
//
//   - longer overall spin-up
//   - more revolutions around the ring
//
// SPIN_START_DELAY_MS:
//
//   Initial delay between moving the energy head one pixel.
//
// Larger value = slower start.
//
// SPIN_END_DELAY_MS:
//
//   Final delay immediately before the white blast.
//
// Smaller value = faster final spin.
//
// ============================================================================

int spinStep = 0;

const int SPIN_STEPS = 48;

const unsigned long SPIN_START_DELAY_MS = 150;
const unsigned long SPIN_END_DELAY_MS   = 10;

// ============================================================================
// DISCHARGE SETTINGS
// ============================================================================
//
// After the white blast, the ring runs a fast colored rotating energy effect.
//
// DISCHARGE_STEPS:
//
//   Number of discharge animation frames.
//
// DISCHARGE_FRAME_MS:
//
//   Constant frame timing during discharge.
//
// ============================================================================

int dischargeStep = 0;

const int DISCHARGE_STEPS = 30;
const unsigned long DISCHARGE_FRAME_MS = 18;

// ============================================================================
// DECAY SETTINGS
// ============================================================================
//
// After discharge, the full ring fades down toward darkness.
//
// decayLevel begins at 220 and decreases by 10 each frame.
//
// ============================================================================

int decayLevel = 220;

const unsigned long DECAY_FRAME_MS = 18;

// ============================================================================
// FULL-WHITE BLAST SETTINGS
// ============================================================================
//
// During this period:
//
//   ring brightness = 255
//   R = 255
//   G = 255
//   B = 255
//
// This is the maximum commanded output from every NeoPixel.
//
// ============================================================================

const unsigned long BLAST_DURATION_MS = 240;

// ============================================================================
// SETUP
// ============================================================================

void setup() {

  // Initialize the NeoPixel library.
  ring.begin();

  // Start with all LEDs off.
  ring.clear();

  // Use the full available global brightness range.
  //
  // Individual animation colors still control actual intensity.
  ring.setBrightness(255);

  // Push the cleared frame to the ring.
  ring.show();

  // Configure all momentary switches using the MCU's internal pull-ups.
  //
  // This means each switch connects its pin to GND when pressed.
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(COLOR_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DIRECTION_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CURVE_BUTTON_PIN, INPUT_PULLUP);

  // Restore the last saved configuration from EEPROM.
  //
  // If EEPROM is invalid/uninitialized, loadConfig() will establish defaults
  // and immediately save them.
  loadConfig();

  // Populate the RGB palette associated with the restored portal color.
  setPortalPalette();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
//
// The loop does two things repeatedly:
//
//   1. Check for button presses.
//   2. Render/update whichever animation state is currently active.
//
// Because the animations mostly use millis() rather than delay(), the buttons
// remain responsive while animations are running.
//
// ============================================================================

void loop() {

  // Handle momentary switches and debounce them.
  updateButtons();

  // Run exactly one animation state.
  switch (animationState) {

    case STATE_IDLE:
      updateIdle();
      break;

    case STATE_SPINUP:
      updateSpinup();
      break;

    case STATE_BLAST:
      updateBlast();
      break;

    case STATE_DISCHARGE:
      updateDischarge();
      break;

    case STATE_DECAY:
      updateDecay();
      break;
  }
}

// ============================================================================
// EEPROM: LOAD CONFIGURATION
// ============================================================================

void loadConfig() {

  // Read the entire Config structure beginning at EEPROM address 0.
  EEPROM.get(0, config);

  // Check magic, version, enum ranges, and direction.
  if (!isConfigValid()) {

    // EEPROM is invalid or has never been initialized.
    //
    // Establish safe defaults.
    portalColor = BLUE_PORTAL;
    accelerationCurve = CURVE_LINEAR;
    dischargeDirection = -1;

    // Save those defaults so the next boot reads valid config.
    saveConfig();

    return;
  }

  // EEPROM passed validation.
  //
  // Restore saved values into the runtime variables.

  portalColor =
    static_cast<PortalColor>(
      config.portalColor
    );

  accelerationCurve =
    static_cast<AccelerationCurve>(
      config.accelerationCurve
    );

  dischargeDirection =
    config.dischargeDirection;
}

// ============================================================================
// EEPROM: VALIDATE CONFIGURATION
// ============================================================================

bool isConfigValid() {

  // Wrong magic means this is not our configuration structure.
  if (config.magic != CONFIG_MAGIC) {
    return false;
  }

  // Wrong version means the EEPROM layout belongs to a different version of
  // this program.
  if (config.version != CONFIG_VERSION) {
    return false;
  }

  // PortalColor currently has only:
  //
  //   0 = BLUE_PORTAL
  //   1 = ORANGE_PORTAL
  //
  if (config.portalColor > ORANGE_PORTAL) {
    return false;
  }

  // Any accelerationCurve value at or above CURVE_COUNT is invalid.
  if (config.accelerationCurve >= CURVE_COUNT) {
    return false;
  }

  // Direction must be exactly +1 or -1.
  if (
    config.dischargeDirection != 1 &&
    config.dischargeDirection != -1
  ) {
    return false;
  }

  return true;
}

// ============================================================================
// EEPROM: SAVE CONFIGURATION
// ============================================================================
//
// Called whenever a persistent user setting changes:
//
//   D10 -> portal color
//   D11 -> discharge direction
//   D12 -> acceleration curve
//
// EEPROM.put() is preferable to blindly writing each byte because Arduino's
// implementation avoids rewriting bytes unnecessarily when possible.
//
// ============================================================================

void saveConfig() {

  // Fill the configuration structure with the current runtime settings.

  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;

  config.portalColor =
    static_cast<uint8_t>(
      portalColor
    );

  config.accelerationCurve =
    static_cast<uint8_t>(
      accelerationCurve
    );

  config.dischargeDirection =
    dischargeDirection;

  // Write the structure starting at EEPROM address 0.
  EEPROM.put(
    0,
    config
  );
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================
//
// All four inputs use the same basic debounce algorithm:
//
//   1. Read the physical pin.
//   2. If the electrical reading changed, reset its debounce timer.
//   3. If it remains stable long enough, accept the new state.
//   4. Respond only when the accepted state becomes LOW.
//
// Responding only on the HIGH -> LOW transition means:
//
//   holding a button down does NOT repeatedly fire/toggle.
//
// ============================================================================

void updateButtons() {

  unsigned long now = millis();

  // ==========================================================================
  // TRIGGER BUTTON - D7
  // ==========================================================================

  bool triggerReading =
    digitalRead(TRIGGER_PIN);

  // Electrical reading changed.
  //
  // Begin/restart debounce timing.
  if (triggerReading != lastTriggerReading) {
    triggerDebounceTime = now;
  }

  // Has the signal remained unchanged long enough to trust it?
  if (
    (now - triggerDebounceTime) >
    DEBOUNCE_MS
  ) {

    // Has the stable state actually changed?
    if (triggerReading != triggerState) {

      triggerState = triggerReading;

      // LOW means the button was pressed.
      if (triggerState == LOW) {
        fire();
      }
    }
  }

  lastTriggerReading =
    triggerReading;

  // ==========================================================================
  // PORTAL COLOR BUTTON - D10
  // ==========================================================================

  bool colorReading =
    digitalRead(COLOR_BUTTON_PIN);

  if (colorReading != lastColorReading) {
    colorDebounceTime = now;
  }

  if (
    (now - colorDebounceTime) >
    DEBOUNCE_MS
  ) {

    if (colorReading != colorButtonState) {

      colorButtonState = colorReading;

      if (colorButtonState == LOW) {
        togglePortalColor();
      }
    }
  }

  lastColorReading =
    colorReading;

  // ==========================================================================
  // DISCHARGE DIRECTION BUTTON - D11
  // ==========================================================================

  bool directionReading =
    digitalRead(DIRECTION_BUTTON_PIN);

  if (
    directionReading !=
    lastDirectionReading
  ) {
    directionDebounceTime = now;
  }

  if (
    (now - directionDebounceTime) >
    DEBOUNCE_MS
  ) {

    if (
      directionReading !=
      directionButtonState
    ) {

      directionButtonState =
        directionReading;

      if (directionButtonState == LOW) {
        toggleDischargeDirection();
      }
    }
  }

  lastDirectionReading =
    directionReading;

  // ==========================================================================
  // ACCELERATION CURVE BUTTON - D12
  // ==========================================================================

  bool curveReading =
    digitalRead(CURVE_BUTTON_PIN);

  if (curveReading != lastCurveReading) {
    curveDebounceTime = now;
  }

  if (
    (now - curveDebounceTime) >
    DEBOUNCE_MS
  ) {

    if (curveReading != curveButtonState) {

      curveButtonState = curveReading;

      if (curveButtonState == LOW) {
        toggleAccelerationCurve();
      }
    }
  }

  lastCurveReading =
    curveReading;
}

// ============================================================================
// TRIGGER EVENT
// ============================================================================
//
// Start a firing sequence only if the prop is currently idle.
//
// Presses during:
//
//   spin-up
//   blast
//   discharge
//   decay
//
// are intentionally ignored.
//
// ============================================================================

void fire() {

  if (animationState != STATE_IDLE) {
    return;
  }

  // Begin spin-up.
  animationState = STATE_SPINUP;

  // Start the rotating energy head from frame zero.
  spinStep = 0;

  // Start timing the first spin frame.
  lastFrameTime = millis();
}

// ============================================================================
// PORTAL COLOR TOGGLE - D10
// ============================================================================

void togglePortalColor() {

  // Don't alter palette while an active firing animation is underway.
  if (animationState != STATE_IDLE) {
    return;
  }

  // Toggle between the two portal palettes.
  if (portalColor == BLUE_PORTAL) {
    portalColor = ORANGE_PORTAL;
  }
  else {
    portalColor = BLUE_PORTAL;
  }

  // Replace the active RGB palette with the newly selected one.
  setPortalPalette();

  // Persist selection through power cycles.
  saveConfig();

  // Restart the idle pulse cleanly from its first endpoint.
  idleStep = 0;
  idleDirection = 1;

  // Force idle rendering to resume immediately.
  lastFrameTime = 0;
}

// ============================================================================
// DISCHARGE DIRECTION TOGGLE - D11
// ============================================================================

void toggleDischargeDirection() {

  // Direction can only be changed while idle.
  if (animationState != STATE_IDLE) {
    return;
  }

  // Flip:
  //
  //   +1 -> -1
  //   -1 -> +1
  //
  dischargeDirection *= -1;

  // Save immediately to EEPROM.
  saveConfig();
}

// ============================================================================
// ACCELERATION CURVE TOGGLE - D12
// ============================================================================

void toggleAccelerationCurve() {

  // Curve selection only occurs while idle.
  if (animationState != STATE_IDLE) {
    return;
  }

  // Advance to the next enum value.
  //
  // Modulo CURVE_COUNT causes the last item to wrap around:
  //
  // Smoothstep -> Linear
  //
  accelerationCurve =
    static_cast<AccelerationCurve>(
      (accelerationCurve + 1) %
      CURVE_COUNT
    );

  // Save the newly selected curve.
  saveConfig();

  // Enum values are zero-based:
  //
  //   Linear      = 0
  //   Quadratic   = 1
  //   Cubic       = 2
  //   Exponential = 3
  //   Smoothstep  = 4
  //
  // Add one so the visible blink count is:
  //
  //   1 through 5
  //
  int blinkCount =
    static_cast<int>(
      accelerationCurve
    ) + 1;

  blinkCurveIndicator(
    blinkCount
  );

  // Return the idle pulse to its beginning afterward.
  idleStep = 0;
  idleDirection = 1;

  lastFrameTime = millis();
}

// ============================================================================
// CURVE SELECTION INDICATOR
// ============================================================================
//
// Only pixel 0 lights.
//
// Example:
//
//   Quadratic selected:
//
//     GREEN
//     off
//     GREEN
//     off
//
// This function intentionally uses delay().
//
// That is acceptable here because:
//
//   - it is a very short user-feedback animation
//   - curve changes are only allowed while idle
//
// ============================================================================

void blinkCurveIndicator(int count) {

  for (
    int blink = 0;
    blink < count;
    blink++
  ) {

    // Start with all pixels off.
    ring.clear();

    // Light pixel zero green.
    ring.setPixelColor(
      CURVE_INDICATOR_PIXEL,
      ring.Color(
        CURVE_INDICATOR_COLOR.r,
        CURVE_INDICATOR_COLOR.g,
        CURVE_INDICATOR_COLOR.b
      )
    );

    ring.show();

    delay(
      CURVE_BLINK_ON_MS
    );

    // Turn the indicator back off.
    ring.clear();
    ring.show();

    delay(
      CURVE_BLINK_OFF_MS
    );
  }
}

// ============================================================================
// PORTAL COLOR PALETTES
// ============================================================================

void setPortalPalette() {

  if (portalColor == BLUE_PORTAL) {

    // ------------------------------------------------------------------------
    // BLUE / PURPLE PORTAL
    // ------------------------------------------------------------------------

    // Strong saturated purple.
    //
    // Increased red component makes the purple endpoint visually distinct
    // from the blue endpoint.
    idleColor1 = {
      140,
      0,
      255
    };

    // Saturated portal blue.
    idleColor2 = {
      0,
      90,
      255
    };

    // Medium-energy blue.
    mediumColor = {
      20,
      30,
      160
    };

    // Bright blue energy.
    hotColor = {
      70,
      120,
      255
    };

    // Near-white blue flash.
    flashColor = {
      160,
      210,
      255
    };
  }
  else {

    // ------------------------------------------------------------------------
    // ORANGE / YELLOW PORTAL
    // ------------------------------------------------------------------------

    // Saturated orange.
    idleColor1 = {
      255,
      70,
      0
    };

    // Warm yellow-orange.
    idleColor2 = {
      255,
      190,
      0
    };

    // Medium orange.
    mediumColor = {
      180,
      45,
      0
    };

    // Bright orange.
    hotColor = {
      255,
      130,
      0
    };

    // Yellow-white flash.
    flashColor = {
      255,
      220,
      80
    };
  }
}

// ============================================================================
// IDLE ANIMATION
// ============================================================================
//
// Repeatedly interpolates:
//
//   idleColor1 -> idleColor2 -> idleColor1
//
// The entire ring is always the same color during idle.
//
// ============================================================================

void updateIdle() {

  unsigned long now = millis();

  // Don't render another frame until enough time has passed.
  if (
    now - lastFrameTime <
    IDLE_FRAME_MS
  ) {
    return;
  }

  lastFrameTime = now;

  // Calculate the intermediate color for the current idle step.
  RGB color =
    blendColors(
      idleColor1,
      idleColor2,
      idleStep,
      IDLE_STEPS
    );

  // Set the entire ring to that color.
  fillRing(color);

  // Move one interpolation step in the current direction.
  idleStep +=
    idleDirection;

  // Reached color #2.
  //
  // Reverse direction and head back toward color #1.
  if (
    idleStep >=
    IDLE_STEPS
  ) {

    idleStep =
      IDLE_STEPS;

    idleDirection = -1;
  }

  // Reached color #1 again.
  //
  // Reverse direction again.
  if (idleStep <= 0) {

    idleStep = 0;
    idleDirection = 1;
  }
}

// ============================================================================
// SPIN-UP ANIMATION
// ============================================================================
//
// A bright energy head travels around the ring.
//
// Layout:
//
//                    head
//                     *
//                  trail1
//                   *
//                trail2
//                 *
//
// The frame delay gets shorter over time, producing acceleration.
//
// At the same time, the head/trail brightness increases.
//
// ============================================================================

void updateSpinup() {

  unsigned long now = millis();

  // Determine how long the current frame should remain visible.
  //
  // This is where the selected acceleration curve is applied.
  unsigned long frameDelay =
    calculateSpinDelay(
      spinStep
    );

  // Wait until the current frame has existed long enough.
  if (
    now - lastFrameTime <
    frameDelay
  ) {
    return;
  }

  lastFrameTime = now;

  // --------------------------------------------------------------------------
  // BACKGROUND ENERGY
  // --------------------------------------------------------------------------
  //
  // Give every pixel a dim version of the primary portal color rather than
  // allowing the inactive pixels to go completely black.
  //
  RGB background =
    scaleColor(
      idleColor1,
      40
    );

  fillRingNoShow(
    background
  );

  // --------------------------------------------------------------------------
  // ROTATING ENERGY HEAD
  // --------------------------------------------------------------------------

  // Head advances one physical LED per animation step.
  int head =
    spinStep %
    NUM_PIXELS;

  // One pixel behind the head.
  int trail1 =
    (
      head +
      NUM_PIXELS -
      1
    ) % NUM_PIXELS;

  // Two pixels behind the head.
  int trail2 =
    (
      head +
      NUM_PIXELS -
      2
    ) % NUM_PIXELS;

  // --------------------------------------------------------------------------
  // BRIGHTNESS RAMP
  // --------------------------------------------------------------------------
  //
  // The rotating energy becomes brighter as the blast approaches.
  //
  // spinStep:
  //
  //   0                  -> brightness 50
  //   SPIN_STEPS - 1     -> brightness 255
  //
  int level =
    map(
      spinStep,
      0,
      SPIN_STEPS - 1,
      50,
      255
    );

  // Scale each palette color according to the current brightness level.
  RGB hot =
    scaleColor(
      hotColor,
      level
    );

  RGB medium =
    scaleColor(
      mediumColor,
      level
    );

  RGB dimTrail =
    scaleColor(
      mediumColor,
      level / 3
    );

  // Brightest moving pixel.
  ring.setPixelColor(
    head,
    ring.Color(
      hot.r,
      hot.g,
      hot.b
    )
  );

  // First trailing pixel.
  ring.setPixelColor(
    trail1,
    ring.Color(
      medium.r,
      medium.g,
      medium.b
    )
  );

  // Dim second trailing pixel.
  ring.setPixelColor(
    trail2,
    ring.Color(
      dimTrail.r,
      dimTrail.g,
      dimTrail.b
    )
  );

  // Send this frame to the physical LEDs.
  ring.show();

  // Advance to the next animation frame.
  spinStep++;

  // --------------------------------------------------------------------------
  // SPIN-UP COMPLETE
  // --------------------------------------------------------------------------

  if (
    spinStep >=
    SPIN_STEPS
  ) {

    // Transition into the full-white blast state.
    animationState =
      STATE_BLAST;

    // Remember exactly when the blast began.
    stateStartTime =
      millis();

    // Global ring brightness at maximum.
    ring.setBrightness(
      255
    );

    // Set every RGB channel of every LED to maximum.
    for (
      int i = 0;
      i < NUM_PIXELS;
      i++
    ) {

      ring.setPixelColor(
        i,
        ring.Color(
          255,
          255,
          255
        )
      );
    }

    ring.show();
  }
}

// ============================================================================
// CALCULATE SPIN FRAME DELAY
// ============================================================================
//
// This function is responsible for the "feel" of the wind-up.
//
// We first normalize the current step:
//
//   progress = 0.0 at beginning
//   progress = 1.0 at end
//
// The selected mathematical curve converts that progress into another value:
//
//   curveValue = 0.0 -> 1.0
//
// That value is then used to interpolate:
//
//   SPIN_START_DELAY_MS
//
// down to:
//
//   SPIN_END_DELAY_MS
//
// Remember:
//
//   larger delay = slower
//   smaller delay = faster
//
// ============================================================================

unsigned long calculateSpinDelay(
  int step
) {

  // Convert integer step into a normalized floating-point value.
  float progress =
    (float)step /
    (float)(
      SPIN_STEPS - 1
    );

  // Default to linear behavior.
  float curveValue =
    progress;

  switch (
    accelerationCurve
  ) {

    // ------------------------------------------------------------------------
    // LINEAR
    //
    // y = x
    //
    // Delay decreases at a uniform rate.
    //
    // ------------------------------------------------------------------------

    case CURVE_LINEAR:

      curveValue =
        progress;

      break;

    // ------------------------------------------------------------------------
    // QUADRATIC
    //
    // y = x^2
    //
    // For values between 0 and 1:
    //
    //   x^2 < x
    //
    // therefore the delay remains closer to the slow starting delay for
    // longer, then decreases more rapidly near the end.
    //
    // ------------------------------------------------------------------------

    case CURVE_QUADRATIC:

      curveValue =
        progress *
        progress;

      break;

    // ------------------------------------------------------------------------
    // CUBIC
    //
    // y = x^3
    //
    // Even more pronounced than quadratic.
    //
    // It spends much of the spin-up moving slowly, then accelerates very
    // rapidly as it approaches the blast.
    //
    // ------------------------------------------------------------------------

    case CURVE_CUBIC:

      curveValue =
        progress *
        progress *
        progress;

      break;

    // ------------------------------------------------------------------------
    // EXPONENTIAL
    //
    // Normalized exponential curve.
    //
    // EXP_STRENGTH determines how aggressive the curve is.
    //
    // Larger values:
    //
    //   stay slow longer
    //   accelerate more violently at the end
    //
    // ------------------------------------------------------------------------

    case CURVE_EXPONENTIAL: {

      const float EXP_STRENGTH =
        5.0;

      // Raw exponential curve shifted so its starting value is zero.
      float numerator =
        exp(
          EXP_STRENGTH *
          progress
        ) - 1.0;

      // Normalize the curve so the ending value becomes exactly 1.0.
      float denominator =
        exp(
          EXP_STRENGTH
        ) - 1.0;

      curveValue =
        numerator /
        denominator;

      break;
    }

    // ------------------------------------------------------------------------
    // SMOOTHSTEP
    //
    // y = 3x^2 - 2x^3
    //
    // Gives an eased transition rather than an abrupt mathematical ramp.
    //
    // ------------------------------------------------------------------------

    case CURVE_SMOOTHSTEP:

      curveValue =
        progress *
        progress *
        (
          3.0 -
          (
            2.0 *
            progress
          )
        );

      break;

    // Safety fallback.
    //
    // Should never happen if EEPROM validation and enum handling are correct.
    default:

      curveValue =
        progress;

      break;
  }

  // Total difference between slowest and fastest frame durations.
  float delayRange =
    SPIN_START_DELAY_MS -
    SPIN_END_DELAY_MS;

  // Convert the curve's 0.0 -> 1.0 output into an actual frame delay.
  //
  // Beginning:
  //
  //   curveValue ~= 0
  //   result ~= SPIN_START_DELAY_MS
  //
  // End:
  //
  //   curveValue ~= 1
  //   result ~= SPIN_END_DELAY_MS
  //
  unsigned long result =
    SPIN_START_DELAY_MS -
    (
      delayRange *
      curveValue
    );

  return result;
}

// ============================================================================
// FULL-WHITE BLAST
// ============================================================================
//
// updateSpinup() already turned every LED full white.
//
// This state simply holds that frame for BLAST_DURATION_MS.
//
// ============================================================================

void updateBlast() {

  unsigned long now =
    millis();

  // Continue holding the white blast until its duration expires.
  if (
    now -
    stateStartTime <
    BLAST_DURATION_MS
  ) {
    return;
  }

  // Blast finished.
  //
  // Move into the rotating colored discharge.
  animationState =
    STATE_DISCHARGE;

  // Start discharge from its first frame.
  dischargeStep = 0;

  // Force first discharge frame immediately.
  lastFrameTime = 0;
}

// ============================================================================
// DISCHARGE / UNWIND
// ============================================================================
//
// This animation runs at a constant high speed.
//
// dischargeDirection controls which direction the energy rotates.
//
// If:
//
//   dischargeDirection = +1
//
// the head advances:
//
//   0,1,2,3,4...
//
// If:
//
//   dischargeDirection = -1
//
// it advances:
//
//   0,11,10,9,8...
//
// wrapPixel() handles negative values safely.
//
// ============================================================================

void updateDischarge() {

  unsigned long now =
    millis();

  // Maintain a constant discharge frame rate.
  if (
    now -
    lastFrameTime <
    DISCHARGE_FRAME_MS
  ) {
    return;
  }

  lastFrameTime =
    now;

  // Unlike spin-up, discharge starts with a completely black background.
  ring.clear();

  // Calculate the current head position in the selected direction.
  int head =
    wrapPixel(
      dischargeDirection *
      dischargeStep
    );

  // One LED ahead of the head.
  int leading =
    wrapPixel(
      head +
      dischargeDirection
    );

  // One LED behind.
  int trail1 =
    wrapPixel(
      head -
      dischargeDirection
    );

  // Two LEDs behind.
  int trail2 =
    wrapPixel(
      head -
      (
        2 *
        dischargeDirection
      )
    );

  // Very bright head.
  ring.setPixelColor(
    head,
    ring.Color(
      flashColor.r,
      flashColor.g,
      flashColor.b
    )
  );

  // Bright leading pixel.
  ring.setPixelColor(
    leading,
    ring.Color(
      hotColor.r,
      hotColor.g,
      hotColor.b
    )
  );

  // Bright trailing pixel.
  ring.setPixelColor(
    trail1,
    ring.Color(
      hotColor.r,
      hotColor.g,
      hotColor.b
    )
  );

  // Dimmer second trail.
  ring.setPixelColor(
    trail2,
    ring.Color(
      mediumColor.r,
      mediumColor.g,
      mediumColor.b
    )
  );

  ring.show();

  dischargeStep++;

  // After all configured discharge frames have run, begin decay.
  if (
    dischargeStep >=
    DISCHARGE_STEPS
  ) {

    animationState =
      STATE_DECAY;

    // Start decay fairly bright.
    decayLevel = 220;

    // Render first decay frame immediately.
    lastFrameTime = 0;
  }
}

// ============================================================================
// DECAY
// ============================================================================
//
// The full ring becomes progressively dimmer using the portal's hotColor.
//
// decayLevel:
//
//   220
//   210
//   200
//   ...
//   10
//   0
//
// Then the state machine returns to idle.
//
// ============================================================================

void updateDecay() {

  unsigned long now =
    millis();

  if (
    now -
    lastFrameTime <
    DECAY_FRAME_MS
  ) {
    return;
  }

  lastFrameTime =
    now;

  // Produce a dimmed version of the current portal color.
  RGB color =
    scaleColor(
      hotColor,
      decayLevel
    );

  // Entire ring uses that decaying color.
  fillRing(color);

  // Reduce brightness for next frame.
  decayLevel -= 10;

  if (
    decayLevel <= 0
  ) {

    // Firing sequence is complete.
    animationState =
      STATE_IDLE;

    // Restart idle animation from its first endpoint.
    idleStep = 0;
    idleDirection = 1;

    // Let idle render immediately.
    lastFrameTime = 0;
  }
}

// ============================================================================
// HELPER: WRAP PIXEL INDEX
// ============================================================================
//
// Ensures any pixel number ends up in:
//
//   0 ... NUM_PIXELS - 1
//
// Particularly important for negative-direction animation.
//
// Example:
//
//   wrapPixel(-1)
//
// becomes:
//
//   11
//
// on a 12-pixel ring.
//
// ============================================================================

int wrapPixel(
  int pixel
) {

  // Reduce large positive/negative values into roughly the correct range.
  pixel %=
    NUM_PIXELS;

  // C/C++ modulo can leave negative results.
  //
  // Convert them into a valid positive index.
  if (pixel < 0) {
    pixel +=
      NUM_PIXELS;
  }

  return pixel;
}

// ============================================================================
// HELPER: INTERPOLATE BETWEEN TWO RGB COLORS
// ============================================================================
//
// Example:
//
//   from = purple
//   to   = blue
//
// At:
//
//   step = 0
//
// result = purple.
//
// At:
//
//   step = totalSteps
//
// result = blue.
//
// Intermediate steps create the smooth idle color pulse.
//
// ============================================================================

RGB blendColors(
  RGB from,
  RGB to,
  int step,
  int totalSteps
) {

  RGB result;

  // Linearly interpolate red.
  result.r =
    from.r +
    (
      (long)(
        to.r -
        from.r
      ) *
      step
    ) /
    totalSteps;

  // Linearly interpolate green.
  result.g =
    from.g +
    (
      (long)(
        to.g -
        from.g
      ) *
      step
    ) /
    totalSteps;

  // Linearly interpolate blue.
  result.b =
    from.b +
    (
      (long)(
        to.b -
        from.b
      ) *
      step
    ) /
    totalSteps;

  return result;
}

// ============================================================================
// HELPER: SCALE RGB BRIGHTNESS
// ============================================================================
//
// level:
//
//   255 = 100% of supplied RGB values
//   128 = about 50%
//   0   = off
//
// This scales the individual RGB channels without altering NeoPixel's global
// brightness setting.
//
// ============================================================================

RGB scaleColor(
  RGB color,
  int level
) {

  RGB result;

  // Cast to uint16_t before multiplication so:
  //
  //   255 * 255
  //
  // does not overflow an 8-bit integer.
  result.r =
    (
      (uint16_t)
      color.r *
      level
    ) / 255;

  result.g =
    (
      (uint16_t)
      color.g *
      level
    ) / 255;

  result.b =
    (
      (uint16_t)
      color.b *
      level
    ) / 255;

  return result;
}

// ============================================================================
// HELPER: FILL WHOLE RING AND DISPLAY IT
// ============================================================================

void fillRing(
  RGB color
) {

  // Load all 12 pixels into the NeoPixel library's RAM buffer.
  fillRingNoShow(
    color
  );

  // Send that RAM buffer to the physical LEDs.
  ring.show();
}

// ============================================================================
// HELPER: FILL WHOLE RING WITHOUT DISPLAYING IT YET
// ============================================================================
//
// Useful when we want to:
//
//   1. Fill the ring with a background color.
//   2. Modify a few individual pixels.
//   3. Call ring.show() only once.
//
// This avoids unnecessary NeoPixel transmissions.
//
// ============================================================================

void fillRingNoShow(
  RGB color
) {

  for (
    int i = 0;
    i < NUM_PIXELS;
    i++
  ) {

    ring.setPixelColor(
      i,
      ring.Color(
        color.r,
        color.g,
        color.b
      )
    );
  }
}
