// Shared state between the runtime (main.cpp), the encoder and the patterns.
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <string>
#include <FastLED.h>
#include <TaskSchedulerDeclarations.h> // the full <TaskScheduler.h> is included once, in main.cpp

#include "config.h"

enum RenderBuffer
{
    BUFFER1,
    BUFFER2
};

// Patterns render into one of two buffers that are crossfaded into leds[].
// Buffers have one extra slot: index NUM_LEDS (== g_lastSafeIndex) is where the
// lookup tables send coordinates that have no LED, so writes there are harmless.
typedef void (*PatternFunction)(CRGB *ledBuffer);

struct Pattern
{
    const char *name;
    PatternFunction render;
};

extern const Pattern gPatterns[];
extern const uint8_t gPatternCount;

extern uint8_t g_targetBrightness;
extern uint8_t g_currentBrightness;
extern uint16_t g_lastSafeIndex;
extern float g_animationSpeed;
// Animation clock: milliseconds scaled by g_animationSpeed. A double, so it
// keeps millisecond resolution for years of uptime (a float stalls after ~2 days).
extern double g_timeAccumulator;

extern CRGB leds[NUM_LEDS];
extern CRGB buffer1[NUM_LEDS + 1];
extern CRGB buffer2[NUM_LEDS + 1];
extern uint8_t gBuffer1PatternNumber;
extern uint8_t gBuffer2PatternNumber;
extern byte g_buffer1InitNeeded;
extern byte g_buffer2InitNeeded;
extern uint8_t _bufferBlendAmount;
extern RenderBuffer _renderBuffer;

extern Task _taskChangeToBrightness;
extern Task _taskChangePattern;
extern Task _taskEncoderAnimation;

// The animation clock scaled by k and wrapped to 16 bits, the form the
// sin8()/cos8() style pattern math expects. Goes through int64 so the
// wrap-around is well defined (a direct float -> uint16_t cast is not).
inline uint16_t animTime16(double k) { return (uint16_t)(int64_t)(g_timeAccumulator * k); }

void changePattern(int8_t direction = 1);
std::string timeToString();
float fmap(float x, float a, float b, float c, float d);

#endif
