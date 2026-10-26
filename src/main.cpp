// Uzumaru - a Fibonacci spiral LED lamp
// Costyn van Dongen 2024-2025
// https://github.com/costyn
//
// Runs on an M5Stack Atom S3 Lite (or the older Atom Lite) driving a Splendida
// 256 PCB (by Uri Shaked / Wokwi) and a DuPPa I2C RGB rotary encoder. Board
// pins are in boards/*.h. Many of the patterns, the lookup
// tables and the palette playlist come from ldirko's Splendida firmware:
// Yaroslaw Turbin 2021, https://github.com/ldirko/New-Colorful-Splendida-256-firmware-
// See CREDITS.md.
//
// Controls (rotary encoder):
//   click        cycle knob mode: brightness (red) -> speed (cyan) -> playlist (yellow)
//   turn         adjust brightness / speed, or next/previous pattern
//   double click in playlist mode: toggle automatic pattern changes

// TODO
// - Use array of time values to vary the duration of the patterns
// - Randomize the patterns.

#include <TaskScheduler.h> // full implementation, included in this file only

#include "globals.h"
#include "palettes.h"
#include "encoder.h"
#include "audio.h"

void changeToBrightness();
void runPattern();
void autoNextPattern();
void changePalette();
void blendPalette();
void bufferCrossfade();
void bufferBlendDone();
void printPatternAndPalette();
void initializeSerial();
void initializeLEDs();

uint8_t g_targetBrightness = DEFAULT_BRIGHTNESS;
uint8_t g_currentBrightness = 0;
float g_animationSpeed = DEFAULT_ANIMATION_SPEED;
double g_timeAccumulator = 0.0;
uint8_t gBuffer1PatternNumber = 0;
uint8_t gBuffer2PatternNumber = 1;
byte g_buffer1InitNeeded = 1; // Don't set explicity
byte g_buffer2InitNeeded = 1;
CRGB leds[NUM_LEDS];
CRGB buffer1[NUM_LEDS + 1];
CRGB buffer2[NUM_LEDS + 1];
uint16_t g_lastSafeIndex = NUM_LEDS;
CRGBPalette16 gTargetPalette;
RenderBuffer _renderBuffer = BUFFER1;

uint8_t _bufferBlendAmount = 0; // blending between buffers

Scheduler _runner;
// This task smoothly changes the brightness to the target brightness
Task _taskChangeToBrightness(10 * TASK_MILLISECOND, TASK_FOREVER, &changeToBrightness);
// Calls the pattern rendering functions
Task _taskRunPattern(1 * TASK_MILLISECOND, TASK_FOREVER, &runPattern);
// Blends the two animation buffers to crossfade between patterns
Task _taskBufferCrossfade((CROSSFADE_TIME / CROSSFADE_STEPS) * TASK_MILLISECOND, CROSSFADE_STEPS, &bufferCrossfade);
// Changes the palette every SECONDS_PER_PALETTE seconds
Task _taskChangePalette(SECONDS_PER_PALETTE *TASK_SECOND, TASK_FOREVER, &changePalette);
// Smoothly blends during palette changes
Task _taskBlendPalette(BLEND_INTERVAL_MS *TASK_MILLISECOND, TASK_FOREVER, &blendPalette);
// Initiates patttern change every SECONDS_PER_PATTERN seconds
Task _taskChangePattern(SECONDS_PER_PATTERN *TASK_SECOND, TASK_FOREVER, &autoNextPattern);
// Reads the rotary encoder
Task _taskReadEncoder(10 * TASK_MILLISECOND, TASK_FOREVER, &readEncoder);
// Animates the encoder LED, when the encoder is idle (not being turned)
Task _taskEncoderAnimation(25 * TASK_MILLISECOND, TASK_FOREVER, &updateEncoderIdleAnimation);

void setup()
{
  delay(500); // power-up delay; let the encoder start up and other stuff settle
  initializeSerial();
  initializeLEDs();
  encoderSetup();

  _runner.init();
  _runner.addTask(_taskChangeToBrightness);
  _runner.addTask(_taskRunPattern);
  _runner.addTask(_taskChangePalette);
  _runner.addTask(_taskChangePattern);
  _runner.addTask(_taskBlendPalette);
  _runner.addTask(_taskReadEncoder);
  _runner.addTask(_taskEncoderAnimation);

  _runner.addTask(_taskBufferCrossfade);
  _taskBufferCrossfade.setOnDisable(&bufferBlendDone);

  _taskChangeToBrightness.enable();
  _taskRunPattern.enable();
  _taskChangePalette.enable();
  _taskChangePattern.enableDelayed(SECONDS_PER_PATTERN * TASK_SECOND); // Don't do this immediately
  _taskBlendPalette.enable();
  _taskReadEncoder.enable();
  _taskEncoderAnimation.enable();

  // Seed both random generators from the ESP32's hardware RNG
  // (analogRead(32) only worked on the classic ESP32; the S3 has no ADC on GPIO 32)
  randomSeed(esp_random());
  random16_set_seed(random());
  // Pick a random palette on start
  changePalette();
}

void loop()
{
  _runner.execute();
}

void runPattern()
{
  // Update global time accumulator
  // Use g_animationSpeed to control the speed of the patterns
  static unsigned long lastUpdate = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate > 0)
  {
    g_timeAccumulator += (double)(currentMillis - lastUpdate) * g_animationSpeed;
    lastUpdate = currentMillis;
  }
  audioUpdate();
  // Yes, it's a bit inefficient to render both buffers all the time,
  // when most of the time only one is showing, but it's not a big deal
  gPatterns[gBuffer1PatternNumber].render(buffer1);
  gPatterns[gBuffer2PatternNumber].render(buffer2);

  blend(buffer1, buffer2, leds, NUM_LEDS, _bufferBlendAmount);

  FastLED.show();
}

void autoNextPattern()
{
  changePattern(1);
}

// Crossfade to the next (direction 1) or previous (direction -1) pattern.
void changePattern(int8_t direction)
{
  constexpr const char *SGN = "changePattern()";

  bool fading = _taskBufferCrossfade.isEnabled();
  uint8_t activePattern = _renderBuffer == BUFFER1 ? gBuffer1PatternNumber : gBuffer2PatternNumber;
  uint8_t incomingPattern = _renderBuffer == BUFFER1 ? gBuffer2PatternNumber : gBuffer1PatternNumber;

  // While a crossfade is running, step on from the pattern that is fading in,
  // so quick knob turns walk through the playlist instead of repeating one step.
  uint8_t next = fading ? incomingPattern : activePattern;
  do
  {
    next = (next + gPatternCount + direction) % gPatternCount;
  } while (next == activePattern); // never render one pattern into both buffers

  // The incoming buffer gets a new pattern, which may need to reset its state
  if (_renderBuffer == BUFFER1)
  {
    gBuffer2PatternNumber = next;
    g_buffer2InitNeeded = 1;
  }
  else
  {
    gBuffer1PatternNumber = next;
    g_buffer1InitNeeded = 1;
  }
  // say straight away where we're going, not only once the crossfade is done
  Serial.printf("%s: %s: %s -> %u/%u %s (crossfading)\n", timeToString().c_str(), SGN,
                direction > 0 ? "next" : "previous", (unsigned)next + 1, (unsigned)gPatternCount, gPatterns[next].name);
  _taskBufferCrossfade.enableIfNot();
}

void bufferCrossfade()
{
  if (_renderBuffer == BUFFER1)
  {
    _bufferBlendAmount = _taskBufferCrossfade.getRunCounter();
  }
  else
  {
    _bufferBlendAmount = CROSSFADE_STEPS - _taskBufferCrossfade.getRunCounter();
  }
}

void bufferBlendDone()
{
  constexpr const char *SGN = "bufferBlendDone()";
  // Swap the main render buffer
  _renderBuffer = _renderBuffer == BUFFER1 ? BUFFER2 : BUFFER1;
  _taskBufferCrossfade.setIterations(CROSSFADE_STEPS); // reset iterations for next run

  Serial.printf("%s: %s: crossfade done, rendering to buffer %s\n", timeToString().c_str(), SGN, _renderBuffer == BUFFER1 ? "1" : "2");
  printPatternAndPalette();
}

void printPatternAndPalette()
{
  constexpr const char *SGN = "printPatternAndPalette()";
  uint8_t pattern = _renderBuffer == BUFFER1 ? gBuffer1PatternNumber : gBuffer2PatternNumber;
  Serial.printf("%s: %s: Pattern: %s \tPalette: %s\n", timeToString().c_str(), SGN, gPatterns[pattern].name, paletteNames[gCurrentPaletteNumber]);
}

void initializeSerial()
{
  Serial.begin(115200);
}

void initializeLEDs()
{
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setDither(BINARY_DITHER);
  FastLED.clear();
}

// Change Palette Periodically
void changePalette()
{
  gCurrentPaletteNumber = random8(gGradientPaletteCount);
  gTargetPalette = gGradientPalettes[gCurrentPaletteNumber];
  printPatternAndPalette();
}

// Blend Current Palette to Target Palette
void blendPalette()
{
  nblendPaletteTowardPalette(gCurrentPalette, gTargetPalette, BLEND_SPEED);
}

boolean changeToTarget(uint8_t target, uint8_t &current)
{
  if (target < current)
  {
    current--;
  }
  else if (target > current)
  {
    current++;
  }
  return target == current;
}

void changeToBrightness()
{
  constexpr const char *SGN = "ChangeToBrightness()";
  if (changeToTarget(g_targetBrightness, g_currentBrightness))
  {
    _taskChangeToBrightness.disable();
    Serial.printf("%s: %s: Brightness adjusted to %u\n", timeToString().c_str(), SGN, g_currentBrightness);
  }
  FastLED.setBrightness(g_currentBrightness);
}

std::string timeToString()
{
  char myString[20];
  unsigned long nowMillis = millis();
  unsigned int seconds = nowMillis / 1000;
  unsigned int remainder = nowMillis % 1000;
  int days = seconds / 86400;
  seconds %= 86400;
  byte hours = seconds / 3600;
  seconds %= 3600;
  byte minutes = seconds / 60;
  seconds %= 60;
  snprintf(myString, 20, "%02d:%02d:%02d:%02d.%03d", days, hours, minutes, seconds, remainder);
  return std::string(myString);
}

float fmap(float x, float a, float b, float c, float d)
{
  float f = x / (b - a) * (d - c) + c;
  return f;
}
