# Architecture and status

How the Uzumaru firmware works, what the October-November 2026 rework fixed (starting from commit `5795e36`, April 2025),
where things stand and what is still open. Start with **Status and open items** if you are picking up the work.

## How it works

```
TaskScheduler loop
 ├─ runPattern (every 1 ms, i.e. as fast as it can)
 │    audioUpdate()                      beat clock (synthetic or beatsense)
 │    gPatterns[n1].render(buffer1)       both buffers render every frame
 │    gPatterns[n2].render(buffer2)
 │    blend(buffer1, buffer2 -> leds)     _bufferBlendAmount: 0 = buffer1, 255 = buffer2
 │    FastLED.show()                      ~7.7 ms for 256 WS2812B
 ├─ bufferCrossfade (255 steps x 15 ms)   ramps the blend, then swaps roles
 ├─ changePattern (every 123 s, or knob)  puts the next pattern in the hidden buffer
 ├─ changePalette / blendPalette          random palette every 19 s, blended smoothly
 ├─ changeToBrightness                    eases brightness to the target
 └─ readEncoder / encoder idle animation
```

The design is sound and suits the lamp:

- **Two persistent buffers with a crossfade** make every transition smooth, and they let "feedback" patterns (those that
  `nblend` or fade the previous frame) keep their own history.
- **A scaled animation clock** (`g_timeAccumulator`) gives one speed knob for every pattern, including running backwards.
- **Cooperative tasks** keep the knob, the palette blend and the brightness ramp independent of the render loop.

## Fixed in the 2026 rework

| # | Problem | Effect | Fix |
|---|---------|--------|-----|
| 1 | Dependencies unpinned and 18 months stale | The code **no longer compiled** against today's releases: espressif32 8.0 (Arduino-ESP32 3.x) makes `round(int)` ambiguous, and `merge-bin.py` broke on esptool 5 | Fixed both; the project builds on the latest espressif32 (8.0.0: Arduino-ESP32 3.3.12 / IDF 5.5), TaskScheduler and DuPPa `main`; FastLED is latest on the Atom Lite and pinned to 3.9.15 on the Atom S3 (see Status). CI builds weekly to catch the next upstream break |
| 2 | Animation clock was a `float` | Increments get rounded: patterns run ~4% fast after ~12 h and **freeze after ~58 h** of uptime (measured) | `double` clock; `animTime16()` makes the 16-bit wrap explicit (a direct float to `uint16_t` cast is undefined once out of range) |
| 3 | Encoder counter limited to 0..254 with wrap off | After a net ~104 clicks one way, turning that way **stops doing anything** (the chip only reports min/max, which nothing listens to) | Re-centre the counter on every step |
| 4 | Turning the knob back in playlist mode | Also went to the *next* pattern | Counter-clockwise goes to the previous pattern |
| 5 | Pattern change during a crossfade | Fast turns re-picked the same pattern; the new pattern got no state reset | Step from the incoming pattern and set its init flag in `changePattern()` |
| 6 | `rain[]` defined in `patterns.h`, shared by DigitalRain and FireComets | Any second file including the header fails to link; the two patterns corrupt each other if both are on screen | Separate static buffers in `ldirko.cpp` |
| 7 | `Serial.printf("%s", String)` | Prints garbage (passes the object, not its text) | Pass a `const char *` |
| 8 | Encoder INT pin floating without the encoder | A missing or loose encoder (or Wokwi) reads INT as low and fires random events | `INPUT_PULLUP` (INT is open-drain) |
| 9 | Wokwi diagram wired LEDs to GPIO 26 | The simulator showed nothing | GPIO 22, unused parts removed |
| 10 | `patternNames[]` and `gPatterns[]` kept in sync by hand | An off-by-one reorder prints the wrong name, or reads past the array | One `Pattern {name, render}` table built with a macro |
| 11 | Patterns compared `ledBuffer == buffer1` to know when to reset | Every stateful pattern repeated the double-buffer internals | `patternNeedsInit(ledBuffer)` |
| 12 | `<TaskScheduler.h>` (which contains the implementation) included from `patterns.h` | Worked only because one file included it; a second include breaks the link | Included once, in `main.cpp`; others use the declarations header |
| 13 | Dead code | OneButton handlers, an unused power helper with a divide by zero, the sprite renderer, abandoned Swirl variants, unused libraries | Removed |

The restructure is behaviour-preserving: all 23 existing patterns render byte-identical frames before and after the file
split (checked with the desktop renderer). The clock change (#2) alters frames only by rounding (mean difference under
0.07/255). All patterns run clean under AddressSanitizer and UndefinedBehaviorSanitizer at normal, reversed and maximum
speed and after 60 h of simulated uptime.

## Status and open items (November 2026)

**On the lamp:** both boards run the current `main`. The owner reviewed every pattern on an Atom Lite at the power-on
defaults and the pattern guidelines in `AGENTS.md` come from that review. Since then, Bloom's glint (deeper) and palette
choice (bright palettes only) and the default brightness (100) changed; the owner has not commented on those yet.

**Atom S3: FastLED pinned to 3.9.15.** With FastLED 3.10.x (up to 3.10.8, the newest) the S3 sends the LEDs a garbage
signal: every LED full red, the current draw drops the USB connection, the firmware itself runs fine. Bisected on the
lamp: FastLED 3.10 fails with its RMT and SPI drivers and on Arduino-ESP32 2.0.17 and 3.3 alike; Arduino-ESP32 3.3 with
FastLED 3.9.15 works. The Atom Lite works with FastLED 3.10. To unpin, flash a newer FastLED release on an S3 lamp first.
Plan B if FastLED stays broken on the S3: hpwit's I2SClocklessLedDriver for the output only (S3 support via the LCD
peripheral; the owner uses it in Lumifera on a classic ESP32 and Arduino 2.x). The owner does not want a FastLED issue
filed yet.

**Visible differences after the FastLED 3.10 upgrade (Atom Lite):** FireButterfly and fire2021 (FastLED 3.10.2 changed 3D
`inoise8`) and the `nblend`-based patterns (Spiral, Swirl, Flower, cylindrical_Pattern, FireComets, Spiral2, RGBTunnel,
colorwaves, pride: slightly less saturated, `nblend` rounds differently). The S3 on FastLED 3.9.15 shows the old look.

**Boot diagnostics:** `setup()` blanks the LEDs first, then prints the board, LED pin, reset reason (BROWNOUT, PANIC,
WATCHDOG...) and each setup stage; with USB serial it waits up to 2 s for a computer.

**Open:**
- beatsense reading (`-D UZUMARU_BEATSENSE`) compiles but has never talked to a real board.
- Wokwi CI needs a `WOKWI_CLI_TOKEN` secret, which the owner could not set from the iOS app.
- The owner's local PlatformIO had stale global platforms (pioarduino 55.3.34 reinstalling `mklittlefs` on every run,
  official 6.9.0); `platformio.ini` now names `platformio/espressif32` so builds match CI regardless.
- Old branches the session could not delete (proxy policy): `claude/uzumaru-restructure-review-eumy7o` here (identical
  to `main`), and `claude/uzumaru-preview`, `-39`, `-update`, `-tuning` in costyn/costyn.github.io (all merged or superseded).
- The recommendations below.

## Recommendations (not changed)

These change how patterns look or need a test on the lamp, so they are left for you to decide.

1. **Cap the frame rate.** `runPattern` runs every 1 ms, so the frame rate is whatever the current pattern pair allows
   (an estimated 60-90 fps; worth logging on the lamp). Feedback patterns (`nblend(..., 16)`, `fadeToBlackBy`, SoftTwinkles, FireComets, PlasmaBall) and the
   `EVERY_N_MILLISECONDS` ones then run at a speed that depends on which other pattern shares the frame. A fixed 60 fps
   (16 ms task) would make them consistent, but would slow some down from what you're used to. The new Vortex shows the
   alternative: fade by elapsed time (`fadeByTime()`, declared in `patterns.h`).
2. **Render only the visible buffer outside a crossfade.** It halves the CPU per frame (and with item 1 frees time for
   beatsense polling). Needs a `patternNeedsInit` on the incoming buffer, which `changePattern()` now sets.
3. **Some patterns ignore the speed knob**: fire2021 (on purpose), DigitalRain and F_lying (fixed `EVERY_N_MILLISECONDS`),
   SoftTwinkles (per frame), PlasmaBall and FireComets (the speed only shifts a `beatsin8` BPM, so they never run backwards). Worth
   marking per pattern, so the knob LED could show "speed not supported".
4. **hypnoticWaves scales by `g_currentBrightness`** while FastLED also applies the brightness, so it is squared: at
   brightness 100 the pattern peaks at about 15% output. If that's not intended, use 127.5 like hypnoticWings.
5. **Names with a leading underscore at global scope** (`_runner`, `_taskX`, `_renderBuffer`) are reserved identifiers in
   C++. Harmless in practice; rename to `g_` when convenient.
6. **Lookup tables are defined in headers** (`tables.h`, `wled_palettes.h`, `geometry.h`). They are `const`, so each file
   gets its own copy and unused copies are dropped by the linker. Fine at this size; move to `.cpp` files if more files
   start including them.

## Music reactivity (beatsense)

`src/audio.h` is shaped like a beatsense frame (tempo, beat phase, beat-in-bar, levels, onsets, energy, groove):

- **Wiring**: beatsense is an I2C slave at 0x42, so it shares the encoder's bus (pins in `src/boards/`) with no new pins. The
  pull-ups live on the beatsense board. beatsense runs at 400 kHz; if the encoder is happy at that speed too,
  `Wire.setClock(400000)` would make the 18-byte read four times shorter.
- **Code**: `-D UZUMARU_BEATSENSE` makes `audioUpdate()` read and CRC-check a frame each render frame, falling back to the
  synthetic beat when there is no board or no signal. This compiles but has not run against a real board.
- **Patterns**: Heartbeat follows `beatPhase` and `beatInBar` and scales with the bass level; Ripples drops on each beat
  when live. The beat phase is state, so a pattern can draw "where in the beat we are" without missing beats.
- **Next steps**: decide which patterns react (a per-pattern flag), and use `energy`/`groove` to pick calmer or busier
  patterns. If polling costs too much frame time, poll every second frame; the phase is extrapolated on the board.

## Testing without the lamp

- `tools/preview` renders any pattern from the real code (and publishes the web preview); `tools/preview/brightness.py`
  measures average LED output. `.github/workflows/build.yml` builds both boards and the beatsense variant, checks
  `geometry.h`, fails on GPS data in photos and renders every pattern, on each push and weekly.
- None of these can see LED signal timing: the S3 FastLED problem only showed on the lamp.
- **Wokwi**: `wokwi.toml` and `diagram.json` (Wokwi's own Splendida part) simulate the Atom Lite build; see
  `tools/wokwi/README.md`. It needs a `WOKWI_CLI_TOKEN` (wokwi.com, CI tokens). No encoder part, so only the auto playlist.
