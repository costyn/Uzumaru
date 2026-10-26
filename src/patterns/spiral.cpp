// Uzumaru patterns: written for the exact Fibonacci spiral geometry.
//
// Instead of a square or cylindrical lookup grid, these use geometry.h, which
// gives every LED its true position, angle, radius and seed rank. The seed
// rank (0 = centre .. 255 = edge) is the order in which a sunflower grows its
// seeds, each one a golden angle (137.5 deg) further round. That makes the
// spirals you see on the board addressable directly:
//
//   rank % F            which arm of family F an LED is on (F = 8, 13, 21, 34)
//   rank / F            how far along that arm it is (0 = innermost)
//   (rank * q) % F      the arm's position going round the disc, in order
//                       (q is the Fibonacci number two before F: 21 -> 8, 34 -> 13)
//
// On this board the 21 arms wind clockwise going outward and the 34 arms
// counter-clockwise; together they form the visible lattice near the edge.
//
// Uzumaru: Costyn van Dongen, 2026.

#include "patterns.h"
#include "geometry.h"
#include "audio.h"

// The animation clock as a 16-bit phase that turns `hz` times per second at
// the default speed. The speed knob scales it (and can run it backwards).
uint16_t phase16(float hz)
{
    return animTime16(hz * 65536.0 / (DEFAULT_ANIMATION_SPEED * 1000.0));
}

// Elapsed animation time since the previous call, always moving forward, in
// default-speed milliseconds. For patterns with particles that must not run
// backwards when the speed knob goes negative.
float forwardMs(double &last)
{
    double delta = fabs(g_timeAccumulator - last) / (DEFAULT_ANIMATION_SPEED);
    last = g_timeAccumulator;
    return delta > 500 ? 500 : delta; // after a pause, don't fast-forward
}

// Fade a buffer towards black with a half life measured in milliseconds, so
// trail lengths don't depend on the frame rate.
void fadeByTime(CRGB *ledBuffer, float ms, float halfLifeMs, float &carry)
{
    carry += 256.0f * (1.0f - powf(0.5f, ms / halfLifeMs));
    uint8_t amount = carry > 255 ? 255 : (uint8_t)carry;
    carry -= amount;
    if (amount)
        fadeToBlackBy(ledBuffer, NUM_LEDS, amount);
}

// Uzumaki_____________________________________
// A two-armed spiral that turns and pulls you towards the centre. How tightly
// it is wound breathes slowly.

void Uzumaki(CRGB *ledBuffer)
{
    const uint8_t arms = 2;
    uint16_t spin = phase16(0.25f);
    uint16_t hue = phase16(0.02f);
    // 0.6 .. 1.6 turns from the centre to the edge
    uint16_t turnsQ8 = 154 + scale16(sin16(phase16(0.015f)) + 32768, 256);

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t r = ledRadius[i];
        uint16_t phase = (uint16_t)(arms * ledAngle[i]) * 256 + (uint16_t)((uint32_t)r * turnsQ8) + spin;
        uint8_t band = cubicwave8(phase >> 8);
        band = scale8(band, band);                 // sharpen the arms
        uint8_t eye = r < 40 ? r * 6 + 15 : 255;   // dark eye at the centre
        uint8_t index = (phase >> 9) + (r >> 1) + (hue >> 8);
        ledBuffer[i] = ColorFromPalette(gCurrentPalette, index, scale8(band, eye));
    }
}

// Parastichy_____________________________________
// Light runs round the 21 clockwise arms one way and round the 34
// counter-clockwise arms the other. The two families take turns, so the eye
// keeps flipping between the two sets of spirals and the lattice they weave.

void Parastichy(CRGB *ledBuffer)
{
    uint8_t runA = phase16(0.11f) >> 8;
    uint8_t runB = phase16(0.07f) >> 8;
    uint8_t drift = phase16(0.01f) >> 8;
    uint8_t flow = phase16(0.6f) >> 8;
    // the families take turns: A, a short overlap, then B
    uint8_t swing = ease8InOutCubic(ease8InOutCubic(triwave8(phase16(0.035f) >> 8)));
    uint8_t mixA = swing;
    uint8_t mixB = 255 - swing;

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t k = ledRank[i];
        uint8_t armA = (k * 8) % 21;  // 21 arms, numbered in order round the disc
        uint8_t armB = (k * 13) % 34; // 34 arms
        // a few thin arms rotating round the disc: 3 on family A, 4 on family B,
        // sharpened (fourth power) so each band is about one arm wide with crisp edges
        uint8_t a = quadwave8(armA * 3 * 256 / 21 - runA);
        uint8_t b = quadwave8(armB * 4 * 256 / 34 + runB);
        a = scale8(a, a);
        a = scale8(a, a);
        b = scale8(b, b);
        b = scale8(b, b);
        // light also pulses along each arm: outward on A, inward on B
        a = scale8(a, 120 + scale8(sin8((k / 21) * 40 - flow), 135));
        b = scale8(b, 120 + scale8(sin8((k / 34) * 56 + flow), 135));
        CRGB color = ColorFromPalette(gCurrentPalette, drift + k / 4, scale8(a, mixA));
        color += ColorFromPalette(gCurrentPalette, drift + 128 + k / 4, scale8(b, mixB));
        ledBuffer[i] = color;
    }
}

// Bloom_____________________________________
// A sunflower grows seed by seed from the centre, in the order nature places
// them, over a flower that is already there: each growth wipes the previous
// palette away and plants the next one. Between wipes the seeds glint.
// Seed colours step through the palette by the golden ratio, which makes them
// line up along the spiral arms. Bloom picks its own palettes, one per wipe.

static const TProgmemRGBGradientPaletteRef bloomPalette(int32_t generation)
{
    uint32_t h = (uint32_t)generation * 0x9E3779B9u;
    h ^= h >> 16;
    return gGradientPalettes[h % gGradientPaletteCount];
}

void Bloom(CRGB *ledBuffer)
{
    const double cycle = 16000.0 * DEFAULT_ANIMATION_SPEED; // one wipe plus the time until the next, in clock units
    double t = fmod(g_timeAccumulator, cycle);
    if (t < 0)
        t += cycle;
    float u = t / cycle;                                         // 0..1 through the cycle
    int32_t generation = (int32_t)floor(g_timeAccumulator / cycle); // which wipe this is
    TProgmemRGBGradientPaletteRef oldRef = bloomPalette(generation);
    TProgmemRGBGradientPaletteRef newRef = bloomPalette(generation + 1);
    if (newRef == oldRef)
        newRef = bloomPalette(generation + 2);
    CRGBPalette16 oldPalette = oldRef;
    CRGBPalette16 newPalette = newRef;
    uint8_t shimmer = phase16(0.5f) >> 8;
    const float tail = 24;          // seeds over which a new seed settles from white to its colour
    const float growUntil = 0.45f;  // the wipe takes this part of the cycle

    float front = u < growUntil ? u / growUntil * (NUM_LEDS + tail) : NUM_LEDS + tail;
    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t k = ledRank[i];
        uint8_t glint = 160 + scale8(sin8(k * 97 + shimmer), 95);
        float age = front - k;
        CRGB color;
        if (age > 0) // planted in this wipe
        {
            color = ColorFromPalette(newPalette, k * 158 + (generation + 1) * 37, glint); // 158/256 = golden ratio
            if (age < tail)
            {
                uint8_t white = 255 - (uint8_t)(age * 255 / tail);
                color = blend(color, CRGB(glint, glint, glint), white);
            }
        }
        else // still the previous flower
        {
            color = ColorFromPalette(oldPalette, k * 158 + generation * 37, glint);
        }
        ledBuffer[i] = color;
    }
}

// SeedRunners_____________________________________
// Comets race outward along the 21 clockwise arms and inward along the 34
// counter-clockwise ones. The 21 arms are also how the board is wired.

static uint8_t comet(uint16_t clock, uint8_t arm, uint8_t along, uint8_t armLength, bool inward)
{
    const uint8_t tail = 5;                              // in seeds
    uint32_t mix = arm * 0x45d9f3bu;                     // scatter the arms' start times
    mix ^= mix >> 16;
    uint16_t h = clock + (uint16_t)mix;
    int32_t headQ8 = ((uint32_t)h * ((armLength + tail) << 8)) >> 16; // runs 0 .. armLength + tail
    int32_t posQ8 = (inward ? armLength - 1 - along : along) << 8;
    int32_t behind = headQ8 - posQ8;
    if (behind < 0 || behind >= (tail << 8))
        return 0;
    uint8_t v = 255 - behind * 255 / (tail << 8);
    return scale8(v, v);
}

void SeedRunners(CRGB *ledBuffer)
{
    uint16_t clockOut = phase16(0.30f);
    uint16_t clockIn = phase16(0.22f);
    uint8_t drift = phase16(0.015f) >> 8;

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t k = ledRank[i];
        // runners on every other arm only (by the arms' order round the disc): less is more
        bool outArm = ((k * 8) % 21) % 2 == 0;  // 11 of the 21 outward arms
        bool inArm = ((k * 13) % 34) % 2 == 0;  // 17 of the 34 inward arms
        uint8_t out = outArm ? comet(clockOut, k % 21, k / 21, 13, false) : 0;
        uint8_t in = inArm ? comet(clockIn, k % 34, k / 34, 8, true) : 0;
        CRGB color = ColorFromPalette(gCurrentPalette, drift + (k % 21) * 12, out);
        color += ColorFromPalette(gCurrentPalette, drift + 128 + (k % 34) * 7, scale8(in, 200));
        ledBuffer[i] = color;
    }
}

// Ripples_____________________________________
// Drops fall on a pond and send out rings, using the LEDs' true positions.
// With a beatsense board, drops fall on the beat.

struct Drop
{
    uint8_t x, y;  // where it landed (geometry.h coordinates)
    uint8_t color; // palette index
    float age;     // ms since it landed (default speed)
};

void Ripples(CRGB *ledBuffer)
{
    const uint8_t maxDrops = 6;
    const float ringSpeed = 0.045f; // coordinate units per ms
    const float lifeMs = 4000;
    static Drop drops[maxDrops];
    static double lastClock = 0;
    static float untilNextDrop = 0;

    if (patternNeedsInit(ledBuffer))
    {
        for (uint8_t d = 0; d < maxDrops; d++)
            drops[d].age = lifeMs;
        lastClock = g_timeAccumulator;
        untilNextDrop = 0;
    }

    float dt = forwardMs(lastClock);
    for (uint8_t d = 0; d < maxDrops; d++)
        drops[d].age += dt;

    untilNextDrop -= dt;
    bool spawn = audioIsLive() ? audioBeatStarted() : untilNextDrop <= 0;
    if (spawn)
    {
        untilNextDrop = 500 + random16(900);
        uint8_t oldest = 0;
        for (uint8_t d = 1; d < maxDrops; d++)
            if (drops[d].age > drops[oldest].age)
                oldest = d;
        uint8_t led = random8(); // land on a random LED, mostly away from the very edge
        if (ledRadius[led] > 200)
            led = random8();
        drops[oldest] = {ledX[led], ledY[led], random8(), 0};
    }

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        // still water
        CRGB color = ColorFromPalette(gCurrentPalette, 160 + ledRadius[i] / 4, 10);
        for (uint8_t d = 0; d < maxDrops; d++)
        {
            if (drops[d].age >= lifeMs)
                continue;
            int16_t dx = ((int16_t)ledX[i] - drops[d].x) / 2;
            int16_t dy = ((int16_t)ledY[i] - drops[d].y) / 2;
            float dist = 2 * sqrt16(dx * dx + dy * dy);
            float ring = drops[d].age * ringSpeed;
            float fade = 1.0f - drops[d].age / lifeMs;
            float off = fabsf(dist - ring);
            float echo = fabsf(dist - ring * 0.6f);
            float level = 0;
            if (off < 14)
                level += (1 - off / 14) * fade;
            if (echo < 10)
                level += (1 - echo / 10) * fade * 0.4f;
            if (drops[d].age < 150 && dist < 10) // splash
                level += 1.0f - drops[d].age / 150;
            if (level > 0)
                color += ColorFromPalette(gCurrentPalette, (uint8_t)(drops[d].color + (uint8_t)(dist / 3)), level > 1 ? 255 : (uint8_t)(level * 255));
        }
        ledBuffer[i] = color;
    }
}

// Vortex_____________________________________
// Sparks orbit the centre, the inner ones faster, and slowly spiral down the
// drain before reappearing at the rim.

struct Spark
{
    float angle;  // radians
    float radius; // 0 = centre, 1 = rim
    uint8_t color;
};

void Vortex(CRGB *ledBuffer)
{
    const uint8_t count = 12;
    static Spark sparks[count];
    static double lastClock = 0;
    static float fadeCarry = 0;

    if (patternNeedsInit(ledBuffer))
    {
        for (uint8_t s = 0; s < count; s++)
            sparks[s] = {random16() / 10430.0f, 0.2f + random8() / 320.0f, random8()};
        lastClock = g_timeAccumulator;
        fill_solid(ledBuffer, NUM_LEDS, CRGB::Black);
    }

    float dt = forwardMs(lastClock);
    fadeByTime(ledBuffer, dt, 140, fadeCarry);

    for (uint8_t s = 0; s < count; s++)
    {
        Spark &p = sparks[s];
        float omega = 0.0012f / powf(p.radius + 0.12f, 1.5f); // rad per ms, Kepler-like
        p.angle += omega * dt;
        if (p.angle > 2 * PI)
            p.angle -= 2 * PI;
        p.radius -= dt * 0.00004f * (1.4f - p.radius);
        if (p.radius < 0.04f)
            p = {random16() / 10430.0f, 1.0f, (uint8_t)(p.color + 40)};

        int16_t px = 128 + 127 * p.radius * sinf(p.angle);
        int16_t py = 128 - 127 * p.radius * cosf(p.angle);
        CRGB c = ColorFromPalette(gCurrentPalette, p.color + (uint8_t)(p.radius * 90));
        c.nscale8(70 + (uint8_t)(p.radius * 185)); // dimmer near the drain, so the core doesn't wash out
        for (uint16_t i = 0; i < NUM_LEDS; i++)
        {
            int16_t dx = ledX[i] - px;
            int16_t dy = ledY[i] - py;
            uint32_t d2 = dx * dx + dy * dy;
            const uint32_t reach2 = 13 * 13;
            if (d2 < reach2)
            {
                uint8_t level = 255 - d2 * 255 / reach2;
                ledBuffer[i] += CRGB(c).nscale8(scale8(level, level));
            }
        }
    }
}

// Heartbeat_____________________________________
// Each beat sends a ring of light out through the seeds; the first beat of a
// bar is brighter. Follows the music with a beatsense board, otherwise beats
// at a resting pulse that the speed knob sets.

void Heartbeat(CRGB *ledBuffer)
{
    uint16_t beat = g_audio.beatPhase; // 0 = on the beat
    bool downbeat = g_audio.beatInBar == 0;
    uint8_t bass = audioIsLive() ? g_audio.level[0] : 255;
    // ring position: fast at first, easing out towards the rim
    uint8_t eased = ease8InOutQuad(beat >> 8);
    uint32_t ringQ8 = (uint32_t)eased * 300; // 0 .. ~300 in radius units (Q8)
    uint8_t drift = phase16(0.02f) >> 8;
    uint8_t ember = phase16(0.05f) >> 8;

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t k = ledRank[i];
        int16_t off = (int16_t)(ringQ8 >> 8) - ledRadius[i];
        // ring with a soft leading edge and a longer glow behind it
        uint8_t ring = 0;
        if (off >= 0 && off < 45)
            ring = 255 - off * 255 / 45;
        else if (off < 0 && off > -10)
            ring = 255 + off * 255 / 10;
        ring = ease8InOutQuad(ring);
        ring = scale8(ring, 255 - (beat >> 8) / 3); // weaker as it travels
        ring = scale8(ring, bass);
        if (!downbeat)
            ring = scale8(ring, 170);

        // between beats: embers glowing along the 13 counter-clockwise arms
        uint8_t arm = (k * 5) % 13;
        uint8_t glow = scale8(quadwave8(arm * 2 * 256 / 13 + ember + k), 10);

        CRGB color = ColorFromPalette(gCurrentPalette, drift + k / 3, qadd8(ring, glow));
        if (ring > 150)
        {
            uint8_t crest = (ring - 150) * (downbeat ? 2 : 1); // white-hot crest
            color += CRGB(crest, crest, crest);
        }
        ledBuffer[i] = color;
    }
}

// Palette colour at a 16-bit index: interpolates between the 8-bit palette
// positions, for gradients that move slowly across few LEDs.
CRGB paletteColor16(const CRGBPalette16 &palette, uint16_t index, uint8_t brightness)
{
    uint8_t whole = index >> 8;
    CRGB a = ColorFromPalette(palette, whole, brightness);
    CRGB b = ColorFromPalette(palette, whole + 1, brightness);
    return blend(a, b, index & 0xFF);
}

// ColorWheel_____________________________________
// The palette laid round the disc and squeezed and stretched: the gradient
// bunches up on one side and spreads out on the other, the bunching travels
// round, and a swirl winds it up and unwinds it. A seamless rework of
// testCylinderMapping2 (whose cylinder map wrapped at 9 o'clock).

void ColorWheel(CRGB *ledBuffer)
{
    // rates tuned on the lamp: 1.6x the first version, which stepped visibly at the default speed
    uint16_t spin = phase16(0.08f);
    uint16_t squeezeAt = -phase16(0.0325f); // where the gradient bunches up
    // how much it bunches: 0 = even wheel .. ~0.9 = most of it on one side
    uint8_t squeeze = scale8(cubicwave8(phase16(0.05f) >> 8), 230);
    int32_t squeezeK = (int32_t)squeeze * 10430 >> 8; // amount * 65536 / (2 pi), Q8 -> Q16
    // swirl: up to ~0.8 turn from the centre to the rim, winding both ways
    int32_t twist = (int32_t)sin16(phase16(0.034f)) * 200 >> 15;

    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        uint16_t angle = (uint16_t)ledAngle[i] << 8;
        // theta + a * sin(theta - c): monotonic for a < 1, so no folds
        int32_t warp = ((int32_t)sin16(angle - squeezeAt) * squeezeK) >> 15;
        uint16_t phase = angle + warp + ledRadius[i] * twist + spin;
        // up the palette and back down once round the disc: no seam. 16-bit
        // index, so slow movement is smooth instead of stepping 2 palette
        // positions at a time (visible as jitter at low brightness).
        uint16_t tri = phase < 0x8000 ? phase * 2 : (0xFFFF - phase) * 2;
        ledBuffer[i] = paletteColor16(gCurrentPalette, ((uint32_t)tri * 240) >> 8);
    }
}
