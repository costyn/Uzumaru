// Patterns adapted from the Fibonacci256 firmware, adapted for Uzumaru.
//
// Jason Coon / Evil Genius Labs, "ESP8266 + FastLED" web server, branch
// fibonacci256 (commit 7f3ef47d9217ac3dfcd2a309d025987fc0d0ba5a):
// https://github.com/jasoncoon/esp8266-fastled-webserver
// Copyright (C) 2015-2020 Jason Coon - GPL-3.0
//
//   Emitter          from emitterFibonacci() (with antialiasPixelAR())
//   SwirlFibonacci   from swirlFibonacci()
//   PolarNoise       from the idea of drawNoisePolar()
//   Pacifica         from pacifica_fibonacci_loop(), the Fibonacci variation
//                    of Pacifica by Mark Kriegsman and Mary Corey March
//                    (December 2019, FastLED examples, MIT)
//
// Modified by Costyn van Dongen 2026: they draw into a caller-supplied buffer
// (for crossfading), follow the speed-scaled animation clock instead of
// millis() and the web UI's speed value, use gCurrentPalette (except Pacifica,
// which keeps its own), and are written for this board's geometry. Their
// "Fibonacci index" is our ledRank, and our physical LED order differs from
// theirs, so every loop runs over our physical LEDs and looks the rank up.

#include "patterns.h"
#include "geometry.h"

static const float kTwoPi = 6.2831853f; // single precision: PI is a double

// Emitter_____________________________________
// Arcs of colour burst from the centre and race outward, leaving fading
// trails. Each arc is a stretch of seeds (rank) over a span of angle, brightest
// along its middle, and its colour follows how far out it is.

void Emitter(CRGB *ledBuffer)
{
    const uint8_t arcHalf = 32;     // half the angular span (256 = a full turn)
    const uint8_t width = 12;       // radial width of an arc, in seeds
    const uint8_t count = 7;        // arcs alive at once
    const float sparkEveryMs = 20;  // chance to start a new arc this often
    const float refFrameMs = 16;    // draw strength is per this much time
    static uint8_t angle[count];
    static float pos[count];        // inner edge of the arc, in seeds
    static float rate[count];       // seeds per ms
    static uint8_t next = 0;        // which arc the next spark replaces
    static double lastClock = 0;
    static float sparkTimer = 0;
    static float fadeCarry = 0;

    if (patternNeedsInit(ledBuffer))
    {
        for (uint8_t e = 0; e < count; e++)
        {
            angle[e] = random8();
            pos[e] = random8();
            rate[e] = 0.128f + random8(16) * 0.004f;
        }
        next = 0;
        lastClock = g_timeAccumulator;
        sparkTimer = 0;
        fadeCarry = 0;
        fill_solid(ledBuffer, NUM_LEDS, CRGB::Black);
    }

    float dt = forwardMs(lastClock);

    // spark new arcs, about 6 a second, each replacing the oldest
    sparkTimer += dt;
    while (sparkTimer >= sparkEveryMs)
    {
        sparkTimer -= sparkEveryMs;
        if (random8(17) <= 1)
        {
            angle[next] = random8();
            pos[next] = 0;
            rate[next] = 0.128f + random8(16) * 0.004f; // 0.5 .. 0.75 turns of the disc per second
            next = next + 1 < count ? next + 1 : 0;
        }
    }

    fadeByTime(ledBuffer, dt, 200, fadeCarry);

    // the arcs wrap round to the centre again if nothing replaces them first
    uint8_t start[count];
    for (uint8_t e = 0; e < count; e++)
    {
        pos[e] += rate[e] * dt;
        if (pos[e] >= 256)
            pos[e] -= 256;
        start[e] = (uint8_t)pos[e];
    }
    uint8_t strength = dt >= refFrameMs ? 255 : (uint8_t)(dt * 255 / refFrameMs);

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t k = ledRank[i];
        uint8_t a = ledAngle[i];
        for (uint8_t e = 0; e < count; e++)
        {
            if ((uint8_t)(k - start[e]) > width)
                continue;
            uint8_t da = a - angle[e];
            uint8_t adiff = min(da, (uint8_t)(-da));
            if (adiff > arcHalf)
                continue;
            // brightest along the middle of the arc, to nothing at its sides
            uint8_t level = 255 - adiff * 255 / arcHalf;
            ledBuffer[i] += ColorFromPalette(gCurrentPalette, start[e], scale8(level, strength));
        }
    }
}

// SwirlFibonacci_____________________________________
// A three-winged swirl, drawn by distance from a curve that puffs, dents and
// winds up as it turns. The distance uses the seed rank, so it is stretched
// towards the centre where the seeds are sparse.

void SwirlFibonacci(CRGB *ledBuffer)
{
    const float zoom = 2.5f;
    const float wings = 3.0f;
    const float pMin = 0.1f, pMax = 2.0f;   // puff up
    const float dMin = 0.1f, dMax = 2.0f;   // dent
    const float sMin = -3.0f, sMax = 2.0f;  // swirl
    const float gMin = 0.1f, gMax = 0.5f;   // glow
    const float darkness = 240;             // inverse brightness

    // each setting drifts slowly between its limits, on its own period
    auto sweep = [](float hz, float lo, float hi)
    {
        return lo + (sin16(phase16(hz)) + 32768) * (1.0f / 65535) * (hi - lo);
    };
    const float p = sweep(0.0254f, pMin, pMax);
    const float d = sweep(0.0332f, dMin, dMax);
    const float s = sweep(0.0137f, sMin, sMax);
    const float g = sweep(0.0527f, gMin, gMax);
    const float turn = phase16(0.1875f) * (kTwoPi / 65536); // wrapped, so it is exact after any uptime

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        float r = ledRank[i] * (zoom / 256);
        float a = ledAngle[i] * (kTwoPi / 256) + turn;
        float v = r - p + d * sinf(wings * a + s * r * r);
        float c = 255 - darkness * powf(fabsf(v), g);
        c = c < 0 ? 0 : (c > 255 ? 255 : c);
        ledBuffer[i] = ColorFromPalette(gCurrentPalette, (uint8_t)c);
    }
}

// Pacifica_____________________________________
// Gentle, blue-green ocean waves: four layers of waves added together, with
// whitecaps where they line up. The waves run along the seed order, so on
// the disc they show up as sweeping spirals. Keeps its own palettes.
//
// Pacifica Fibonacci variation: Mark Kriegsman and Mary Corey March, December 2019.

static const CRGBPalette16 pacificaPalette1 =
    {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
     0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50};
static const CRGBPalette16 pacificaPalette2 =
    {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
     0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F};
static const CRGBPalette16 pacificaPalette3 =
    {0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
     0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF};

// beatsin16() and friends on the animation clock: `hz` cycles a second at the
// default speed, moving between lo and hi.
static float wave(float hz, float lo, float hi)
{
    return lo + (sin16(phase16(hz)) + 32768) * (1.0f / 65535) * (hi - lo);
}

// Add one layer of waves, in seed order.
static void pacificaLayer(CRGB *seeds, const CRGBPalette16 &p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
    uint16_t ci = cistart;
    uint16_t waveangle = ioff;
    uint16_t wavescale_half = (wavescale / 2) + 20;
    for (uint16_t k = 0; k < NUM_LEDS; k++)
    {
        waveangle += 250;
        uint16_t s16 = sin16(waveangle) + 32768;
        uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
        ci += cs;
        uint16_t sindex16 = sin16(ci) + 32768;
        uint8_t sindex8 = scale16(sindex16, 240);
        seeds[k] += ColorFromPalette(p, sindex8, bri, LINEARBLEND);
    }
}

void Pacifica(CRGB *ledBuffer)
{
    static float ciStart[4]; // where each layer's colours start, 0 .. 65536
    static double lastClock = 0;

    if (patternNeedsInit(ledBuffer))
    {
        for (uint8_t l = 0; l < 4; l++)
            ciStart[l] = 0;
        lastClock = g_timeAccumulator;
    }

    // Elapsed time, signed so that a negative speed runs the waves backwards.
    float dt = (g_timeAccumulator - lastClock) / DEFAULT_ANIMATION_SPEED;
    lastClock = g_timeAccumulator;
    if (dt > 500)
        dt = 500; // after a pause, don't fast-forward
    else if (dt < -500)
        dt = -500;

    // Move the four colour-start counters, each at its own, slowly varying, speed.
    float speedfactor1 = wave(0.05f, 179, 269) / 256;
    float speedfactor2 = wave(0.0667f, 179, 269) / 256;
    float dt1 = dt * speedfactor1;
    float dt2 = dt * speedfactor2;
    float dt21 = (dt1 + dt2) / 2;
    ciStart[0] += dt1 * wave(0.0658f, 10, 13);
    ciStart[1] -= dt21 * wave(0.0506f, 8, 11);
    ciStart[2] -= dt1 * wave(0.0326f, 5, 7);
    ciStart[3] -= dt2 * wave(0.0167f, 4, 6);
    uint16_t ci[4];
    for (uint8_t l = 0; l < 4; l++)
    {
        ciStart[l] = fmodf(ciStart[l], 65536.0f);
        if (ciStart[l] < 0)
            ciStart[l] += 65536.0f;
        ci[l] = (uint16_t)(uint32_t)ciStart[l];
    }

    // Work in seed order, like the original does along its strip, then deal the seeds out to LEDs.
    CRGB seeds[NUM_LEDS];

    // a dim background blue-green
    fill_solid(seeds, NUM_LEDS, CRGB(2, 6, 10));

    // four layers of waves, with different scales and speeds that vary over time
    pacificaLayer(seeds, pacificaPalette1, ci[0], wave(0.05f, 11 * 256, 14 * 256), wave(0.1667f, 70, 130), -phase16(0.0196f));
    pacificaLayer(seeds, pacificaPalette2, ci[1], wave(0.0667f, 6 * 256, 9 * 256), wave(0.2833f, 40, 80), phase16(0.0261f));
    pacificaLayer(seeds, pacificaPalette3, ci[2], 6 * 256, wave(0.15f, 10, 38), -phase16(0.0328f));
    pacificaLayer(seeds, pacificaPalette3, ci[3], 5 * 256, wave(0.1333f, 10, 28), phase16(0.0391f));

    // brighter 'whitecaps' where the waves line up
    uint8_t basethreshold = wave(0.15f, 55, 65);
    uint8_t whitecap = phase16(0.1167f) >> 8;
    for (uint16_t k = 0; k < NUM_LEDS; k++)
    {
        uint8_t threshold = scale8(sin8(whitecap), 20) + basethreshold;
        whitecap += 7;
        uint8_t l = seeds[k].getAverageLight();
        if (l > threshold)
        {
            uint8_t overage = l - threshold;
            uint8_t overage2 = qadd8(overage, overage);
            seeds[k] += CRGB(overage, overage2, qadd8(overage2, overage2));
        }
    }

    // deepen the blues and greens a bit
    for (uint16_t k = 0; k < NUM_LEDS; k++)
    {
        seeds[k].blue = scale8(seeds[k].blue, 145);
        seeds[k].green = scale8(seeds[k].green, 200);
        seeds[k] |= CRGB(2, 5, 7);
        // ~1.8x gain: the original is dim by design, too dark at the lamp's default brightness
        CRGB extra = seeds[k];
        seeds[k] += extra.nscale8(205);
    }

    for (uint16_t i = 0; i < NUM_LEDS; i++)
        ledBuffer[i] = seeds[ledRank[i]];
}

// PolarNoise_____________________________________
// Palette noise flowing in rings and arms. The noise is sampled round a circle
// (x = R cos(angle), y = R sin(angle)) with the distance from the centre on the
// third axis, so there is no seam where the angle wraps. The rings drift
// outward as time moves along that axis, and the arms lean and sway.

void PolarNoise(CRGB *ledBuffer)
{
    const int32_t circle = 220;        // radius of the sampling circle: about 5 arms round the disc
    const int32_t ringsQ4 = 28;        // noise units per unit of radius, Q4: about 1.7 rings from centre to rim
    const uint16_t flow = phase16(0.0022f);  // rings drift outward (0.56 noise cells a second)
    const uint16_t sway = phase16(0.012f);   // arms turn slowly
    const uint16_t leanQ8 = 80 + scale16(sin16(phase16(0.011f)) + 32768, 120); // how far the arms lean, per unit of radius

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t r = ledRadius[i];
        uint16_t a = ((uint16_t)ledAngle[i] << 8) + sway + (r * leanQ8);
        uint16_t x = 32768 + ((circle * cos16(a)) >> 15);
        uint16_t y = 32768 + ((circle * sin16(a)) >> 15);
        uint16_t z = ((r * ringsQ4) >> 4) - flow;

        uint8_t n = inoise8(x, y, z);
        // inoise8 mostly stays within 16 .. 238: stretch that to 0 .. 255
        n = qsub8(n, 16);
        n = qadd8(n, scale8(n, 39));
        ledBuffer[i] = ColorFromPalette(gCurrentPalette, n, 255, LINEARBLEND);
    }
}
