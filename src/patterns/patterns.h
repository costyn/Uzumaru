// Shared declarations for all pattern files.
//
//   ldirko.cpp   patterns from ldirko's Splendida 256 firmware (and Mark
//                Kriegsman's pride/colorwaves), adapted to double buffering
//   mapped.cpp   Costyn's patterns on ldirko's planar/cylindrical maps
//   spiral.cpp   Uzumaru patterns built on the exact LED geometry (geometry.h)
//   fibonacci256.cpp  patterns adapted from Jason Coon's Fibonacci256 firmware
//                (and Mark Kriegsman's Pacifica), on the exact LED geometry
//   wled.cpp     patterns adapted from WLED's 2D effects, on the exact LED geometry
//   playlist.cpp the order patterns are shown in
#ifndef PATTERNS_H
#define PATTERNS_H

#include <Arduino.h>
#include <FastLED.h>

#include "globals.h"
#include "palettes.h"
#include "tables.h"

// Coordinate mapping into the LED buffer (index NUM_LEDS when there is no LED)
uint16_t XY_fibon_PLANAR(byte x, byte y);
uint16_t XY_CYLINDER(byte x, byte y);
void GammaCorrection(CRGB *ledBuffer);

// True once after a pattern has been (re)assigned to ledBuffer, so it can
// reset its state. Use instead of comparing against buffer1/buffer2.
bool patternNeedsInit(CRGB *ledBuffer);

// Time helpers for the geometry patterns (spiral.cpp); all follow the
// speed-scaled animation clock.
// A 16-bit phase that turns `hz` times per second at the default speed.
uint16_t phase16(float hz);
// Elapsed clock time since the last call (updates `last`), always positive, in
// default-speed milliseconds, for particles that must not run backwards.
float forwardMs(double &last);
// Fade a buffer to black with a half life in milliseconds (frame-rate independent).
void fadeByTime(CRGB *ledBuffer, float ms, float halfLifeMs, float &carry);
// Palette colour at a 16-bit index (interpolated): smooth slow gradients, no
// stepping or dither flicker at low brightness.
CRGB paletteColor16(const CRGBPalette16 &palette, uint16_t index, uint8_t brightness = 255);

// ldirko.cpp
void DigitalRain(CRGB *buffer);
void DiagonalPattern(CRGB *buffer);
void fire2021(CRGB *buffer);
void Distortion_Waves_planar(CRGB *buffer);
void RGB_hiphotic(CRGB *buffer);
void PlasmaBall(CRGB *buffer);
void FireComets(CRGB *buffer);
void F_lying(CRGB *buffer);
void RGBTunnel(CRGB *buffer);
void RGB_Caleidoscope1(CRGB *buffer);
void RGB_Caleidoscope2(CRGB *buffer);
void Distortion_Waves_cylinder(CRGB *buffer);
void FireButterfly(CRGB *buffer);
void Swirl(CRGB *buffer);
void SwirlPlanar(CRGB *buffer);
void cylindrical_Pattern(CRGB *buffer);
void Spiral(CRGB *buffer);
void Spiral2(CRGB *buffer);
void Flower(CRGB *buffer);
void pride(CRGB *buffer);
void pridePalette(CRGB *buffer);
void colorwaves(CRGB *buffer);
void SoftTwinkles(CRGB *buffer);

// mapped.cpp
void spiralCylinderWave(CRGB *buffer);
void testCylinderMapping(CRGB *buffer);
void testCylinderMapping2(CRGB *buffer);
void hypnoticWings(CRGB *buffer);
void hypnoticWaves(CRGB *buffer);

// spiral.cpp
void Uzumaki(CRGB *buffer);
void Parastichy(CRGB *buffer);
void Bloom(CRGB *buffer);
void SeedRunners(CRGB *buffer);
void Ripples(CRGB *buffer);
void Vortex(CRGB *buffer);
void Heartbeat(CRGB *buffer);
void ColorWheel(CRGB *buffer);

// fibonacci256.cpp
void Emitter(CRGB *buffer);
void SwirlFibonacci(CRGB *buffer);
void Pacifica(CRGB *buffer);
void PolarNoise(CRGB *buffer);

// wled.cpp
void Octopus(CRGB *buffer);
void PolarLights(CRGB *buffer);
void SunRadiation(CRGB *buffer);
void DriftRose(CRGB *buffer);

#endif
