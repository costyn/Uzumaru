// Uzumaru hardware configuration and tuning constants.
#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware: pins and power budget per controller board ---
// The board is chosen by the PlatformIO environment (see platformio.ini). This
// file is included after FastLED.h: FastLED uses DATA_PIN as a template
// parameter name, so the macro must not exist before FastLED is included.
#if defined(UZUMARU_BOARD_ATOM_S3)
#include "boards/atom_s3.h"
#elif defined(UZUMARU_BOARD_ATOM)
#include "boards/atom.h"
#else
#error "No board selected: build one of the environments in platformio.ini"
#endif

#define LED_TYPE WS2812B // leds type
#define COLOR_ORDER GRB  // color order of leds
#define NUM_LEDS 256

// DuPPa I2C RGB rotary encoder (https://www.duppa.net/)
#define ENCODER_ADDRESS 0x28

// --- Power & brightness ---
#define MIN_BRIGHTNESS 1        // min brightness of leds
#define MAX_BRIGHTNESS 250      // max brightness of leds - will be limited by max power
#define DEFAULT_BRIGHTNESS 100  // brightness at power-on (50 was too dim on the lamp)

// --- Lookup table resolutions (see tables.h) ---
#define NUM_COLS_PLANAR 20 // resolution of planar lookup table
#define NUM_ROWS_PLANAR 20 // resolution of planar lookup table
#define NUM_LEDS_PLANAR NUM_COLS_PLANAR *NUM_ROWS_PLANAR

#define NUM_COLS_CYLINDER 45 // resolution of cylindrical lookup table
#define NUM_ROWS_CYLINDER 11 // resolution of cinindrical lookup table

// --- Animation ---
#define MAX_ANIMATION_SPEED 0.4f
#define MIN_ANIMATION_SPEED -0.4f
#define DEFAULT_ANIMATION_SPEED 0.08f
#define ANIMATION_SPEED_STEP 0.01f

#define SECONDS_PER_PALETTE 19
#define SECONDS_PER_PATTERN 123
#define BLEND_SPEED 16
#define BLEND_INTERVAL_MS 40
#define CROSSFADE_TIME 4000 // milliseconds
#define CROSSFADE_STEPS 255 // 255 = max (8 bit)

// Playlist mode: a knob step back in the other direction within this time is
// treated as detent wobble and ignored
#define PLAYLIST_REVERSE_GUARD_MS 350

// Now you can see how long the crossfade is still active
#define ENCODER_ANIMATION_IDLE_TIMEOUT CROSSFADE_TIME

#endif // CONFIG_H
