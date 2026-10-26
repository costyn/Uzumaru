// The order in which patterns are shown. Add, remove or reorder entries here.
#include "patterns.h"

#define PATTERN(fn) {#fn, fn}

const Pattern gPatterns[] = {
    // Uzumaru patterns (spiral.cpp)
    PATTERN(Uzumaki),
    PATTERN(Parastichy),
    PATTERN(Bloom),
    PATTERN(SeedRunners),
    PATTERN(Ripples),
    PATTERN(Vortex),
    // PATTERN(Heartbeat), // too intense on a loop; kept for one-shot music reactions
    PATTERN(ColorWheel), // replaces testCylinderMapping2
    // Adapted from Jason Coon's Fibonacci256 firmware (fibonacci256.cpp)
    PATTERN(Emitter),
    PATTERN(SwirlFibonacci),
    PATTERN(Pacifica),
    PATTERN(PolarNoise),
    // Adapted from WLED's 2D effects (wled.cpp)
    PATTERN(Octopus),
    PATTERN(PolarLights),
    PATTERN(SunRadiation),
    PATTERN(DriftRose),
    // Map-based patterns (ldirko.cpp, mapped.cpp)
    PATTERN(hypnoticWings),
    PATTERN(hypnoticWaves),
    PATTERN(DigitalRain),
    PATTERN(SoftTwinkles),
    PATTERN(cylindrical_Pattern),
    PATTERN(FireComets),
    PATTERN(Swirl),
    PATTERN(DiagonalPattern),
    PATTERN(spiralCylinderWave),
    // PATTERN(PlasmaBall),
    // PATTERN(F_lying),  // I don't like it enough.
    PATTERN(RGBTunnel),
    PATTERN(Flower),
    PATTERN(Distortion_Waves_cylinder),
    PATTERN(colorwaves),
    PATTERN(Distortion_Waves_planar),
    PATTERN(FireButterfly),
    PATTERN(Spiral2),
    PATTERN(pride),
    PATTERN(pridePalette),
    PATTERN(RGB_Caleidoscope2),
    PATTERN(RGB_Caleidoscope1),
    PATTERN(RGB_hiphotic),
    PATTERN(Spiral),
    PATTERN(fire2021),
};

const uint8_t gPatternCount = sizeof(gPatterns) / sizeof(gPatterns[0]);
