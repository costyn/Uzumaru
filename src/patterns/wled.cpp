// Patterns adapted from WLED's 2D effects, adapted for Uzumaru.
//
// WLED, https://github.com/wled/WLED (commit 3d5c9709703f91297a05c4a9b781d8e4aadfae80),
// wled00/FX.cpp. WLED is licensed EUPL-1.2, which lists GPL-3.0 as a compatible
// licence, so these adaptations are redistributed under this firmware's GPL-3.0.
//
//   Octopus      from mode_2Doctopus(). Idea from
//                https://www.youtube.com/watch?v=HsA-6KIbgto&ab_channel=GreatScott%21
//                Octopus (https://editor.soulmatelights.com/gallery/671-octopus),
//                Stepko and Sutaburosu, adapted for WLED by @blazoncek
//   PolarLights  from mode_2DPolarLights(). By: Kostyantyn Matviyevskyy
//                https://editor.soulmatelights.com/gallery/762-polar-lights ,
//                Modified by: Andrew Tuline & @dedehai (palette support)
//   SunRadiation from mode_2DSunradiation(). By: ldirko
//                https://editor.soulmatelights.com/gallery/599-sun-radiation ,
//                modified by: Andrew Tuline
//   DriftRose    from mode_2Ddriftrose(). Drift Rose by stepko (c)2021
//                [https://editor.soulmatelights.com/gallery/1369-drift-rose-pattern],
//                adapted by Blaz Kristan (AKA blazoncek) improved by @dedehai
//
// Adapted for Uzumaru by Costyn van Dongen 2026 (with Claude): they draw into a
// caller-supplied buffer (for crossfading), follow the speed-scaled animation
// clock instead of strip.now, use gCurrentPalette (except SunRadiation, which
// keeps its HeatColor glow) and read each LED's true position from geometry.h
// instead of a 16x16 pixel grid. WLED's default slider values are built in as
// constants; positions are scaled so the disc reads like a 16x16 matrix.

#include "patterns.h"
#include "geometry.h"

// A counter that advances `perMs` units per millisecond of the animation clock
// (default speed), wrapped to 32 bits. It runs backwards with a negative speed.
static uint32_t clockUnits(float perMs)
{
    return (uint32_t)(int64_t)(g_timeAccumulator * (perMs / DEFAULT_ANIMATION_SPEED));
}

// FastLED's inoise8 mostly stays within 16 .. 238; WLED's perlin8 fills 0 .. 255.
// Stretch it so the thresholds the effects were tuned with still land right.
static uint8_t noise8(uint16_t x, uint16_t y, uint16_t z)
{
    uint8_t n = qsub8(inoise8(x, y, z), 16);
    return qadd8(n, scale8(n, 39));
}

// Octopus_____________________________________
// Five wriggling tentacles around a centre, drawn as interference bands that
// ride the palette outward. The angle and radius come straight from the LED.

void Octopus(CRGB *ledBuffer)
{
    const uint8_t legs = 16 / 4 + 1;            // WLED's "Legs" slider at its default of 16
    // WLED adds 5 per frame (speed 128), about 200 a second; half that is calmer on a lamp
    const uint32_t step = clockUnits(0.1f);

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        int32_t angle = (uint8_t)(ledAngle[i] - 64);       // WLED measures from the right, ours from the top
        int32_t radius = ((uint16_t)ledRadius[i] * 88) >> 8; // 16x16 matrix: 8 pixels to the rim, 11 units each
        uint8_t inner = sin8((uint8_t)((angle * 4 - radius) / 4 + step / 2));
        uint8_t intensity = sin8((uint8_t)(inner + radius - step + angle * legs));
        ledBuffer[i] = ColorFromPalette(gCurrentPalette, (uint8_t)(step / 2 - radius), intensity);
    }
}

// PolarLights_____________________________________
// Aurora curtains: noise stretched into vertical streaks, strongest in a band
// across the middle and fading away towards the top and bottom.

void PolarLights(CRGB *ledBuffer)
{
    // WLED on a 16x16 matrix with its sliders at 128: x*152, y*16 (+ a small
    // per-pixel jitter that works out to about y*17), time at 3.5 per frame (about 140 a second, slowed to 100)
    const uint16_t z = clockUnits(0.1f);
    // brightness lost per pixel away from the middle row: WLED's 23 left most of a
    // round disc dark at the lamp's default brightness
    const uint8_t adjustHeight = 16;

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint16_t x = ((uint16_t)ledX[i] * 152) >> 4;
        uint16_t y = ((uint16_t)ledY[i] * 17) >> 4;
        uint8_t n = noise8(x, y, z);
        uint16_t away = (abs((int16_t)ledY[i] - 128) * adjustHeight) >> 4; // 16 ledY units per pixel
        n = qsub8(n, away > 255 ? 255 : away);
        ledBuffer[i] = ColorFromPalette(gCurrentPalette, n, qadd8(n, n >> 2)); // a little brighter than WLED's
    }
}

// SunRadiation_____________________________________
// A sun seen through shimmering heat: noise is a bumpy surface lit from the
// centre, and the glow is brightest where the surface faces the middle.

void SunRadiation(CRGB *ledBuffer)
{
    const uint8_t someVal = 128 / 4;     // noise units per pixel ("Variance" at 128)
    const uint16_t brightness = 128;     // "Brightness" at 128: heat = glow / 2
    const uint16_t t = clockUnits(0.25f); // WLED: strip.now / 4
    // 16 ledX/ledY units are one matrix pixel
    const uint16_t reach = someVal;      // one pixel either side, for the gradient
    const uint16_t base = 1024;          // keep the offsets positive

    auto bump = [&](uint16_t x, uint16_t y)
    {
        return (int16_t)(((int16_t)noise8(x, y, t) - 127) >> 2); // about +/- 32
    };

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint16_t x = base + (((uint16_t)ledX[i] * someVal) >> 4);
        uint16_t y = base + (((uint16_t)ledY[i] * someVal) >> 4);
        int16_t nx = bump(x + reach, y) - bump(x - reach, y);
        int16_t ny = bump(x, y + reach) - bump(x, y - reach);
        int16_t difx = abs((((int16_t)ledX[i] - 128) * 7 >> 4) - nx);
        int16_t dify = abs((((int16_t)ledY[i] - 128) * 7 >> 4) - ny);
        int32_t glow = 255 - (difx * difx + dify * dify) / 8; // 8 is the size of the sun
        if (glow < 0)
            glow = 0;
        uint16_t heat = glow * 128 / (384 - brightness);
        ledBuffer[i] = HeatColor(heat > 255 ? 255 : heat);
    }
}

// DriftRose_____________________________________
// 36 points, one every 10 degrees, each sliding back and forth along its own
// diameter at its own pace. Together they trace slowly shifting rose curves.

void DriftRose(CRGB *ledBuffer)
{
    // Changed for the lamp: WLED's 36 points each swing at their own rate
    // (1..36 per minute), so after the opening swirl they scatter into many
    // separate bits for most of each minute, too much detail for 256 LEDs.
    // Here 24 points swing at one rate with a per-point phase offset that slowly
    // winds and unwinds: a star burst when it is zero, a rose or spiral when
    // wound, and never a scatter.
    const uint8_t points = 24;
    const float fadeHalfLifeMs = 80;
    const uint32_t reach2 = 15 * 15;      // a point lights the LEDs within this distance
    static double lastClock = 0;
    static float fadeCarry = 0;

    if (patternNeedsInit(ledBuffer))
    {
        lastClock = g_timeAccumulator;
        fadeCarry = 0;
        fill_solid(ledBuffer, NUM_LEDS, CRGB::Black);
    }

    float dt = forwardMs(lastClock);
    fadeByTime(ledBuffer, dt, fadeHalfLifeMs, fadeCarry);

    uint16_t base = phase16(0.25f);
    // phase offset between neighbouring points: up to 1.5 turns over all points, both ways
    int32_t windStep = ((int32_t)sin16(phase16(0.012f)) * (65536 * 3 / 2 / points)) >> 15;

    for (uint8_t p = 0; p < points; p++)
    {
        int16_t swing = ((int32_t)sin16(base + p * windStep) * 128) >> 15;
        uint16_t angle = (uint16_t)((uint32_t)p * 65536 / points);
        int16_t px = 128 + (((int32_t)sin16(angle) * swing) >> 15);
        int16_t py = 128 + (((int32_t)cos16(angle) * swing) >> 15);
        CRGB color = ColorFromPalette(gCurrentPalette, (uint8_t)(p * 256 / points));

        for (uint16_t i = 0; i < NUM_LEDS; i++)
        {
            int16_t dx = ledX[i] - px;
            int16_t dy = ledY[i] - py;
            uint32_t d2 = dx * dx + dy * dy;
            if (d2 < reach2)
            {
                uint8_t level = 255 - d2 * 255 / reach2;
                ledBuffer[i] += CRGB(color).nscale8(scale8(level, level));
            }
        }
    }
}
