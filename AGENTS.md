# Uzumaru: notes for coding agents

Uzumaru is a round LED lamp: 256 WS2812B LEDs on a Fibonacci (Vogel/sunflower) spiral PCB (the Splendida 256), an M5Stack
Atom S3 Lite (ESP32-S3; older lamps: Atom Lite, ESP32-PICO-D4), and one DuPPa I2C RGB rotary encoder as the only control.
PlatformIO, Arduino framework, FastLED, TaskScheduler. Photos, the manual and the live pattern preview are on the owner's
GitHub Pages site, https://costyn.github.io/uzumaru/ (repo costyn/costyn.github.io, folder `uzumaru/`). Read `README.md` for the user-facing picture and `docs/ARCHITECTURE_REVIEW.md` for how the
runtime works and what is still open.

## The owner usually cannot test on hardware

Assume nobody can flash or watch the lamp during a session. So:

- Never say something "works" or was "tested" unless it ran. Say what was checked: compiled for the ESP32, rendered on
  the desktop, ran under sanitizers, simulated in Wokwi. Hardware-only behaviour (LED timing, the encoder, I2C, power)
  stays "untested on hardware" until the owner confirms it.
- Prefer changes you can verify with the tools below. Keep risky runtime changes small, separate, and listed in the
  commit message or PR so the owner knows what to try first when they're back at the lamp.

## Build and verify

```
pio run -e m5stack_atomlite_s3 -e m5stack_atomlite              # firmware for both boards (must stay green)
PLATFORMIO_BUILD_FLAGS=-DUZUMARU_BEATSENSE pio run -e m5stack_atomlite_s3   # with the beatsense reader
tools/preview/build.sh                                           # desktop renderer -> .pio/preview/preview
python3 tools/preview/make_preview.py png out.png <Pattern> 1 3 6     # stills to look at
python3 tools/preview/make_preview.py html out.html [Pattern ...]     # animated page (publish as an artifact for the owner)
python3 tools/gen_geometry.py > src/geometry.h                   # only if tables.h changes; CI checks it is current
tools/preview/publish.sh ../costyn.github.io                     # refresh the public preview (then commit there)
tools/wokwi/smoke.sh                                             # Wokwi boot test (needs WOKWI_CLI_TOKEN)
```

- The renderer compiles the real pattern sources against the real FastLED (stub platform) with a simulated clock. Use it
  to look at every pattern you write or change: render PNG stills at a few times and actually view them.
- **Refactors must be behaviour-preserving**: render every pattern before and after (`preview <pattern> 6 7 12 1 > x.bin`)
  and compare the bytes. A pure refactor gives identical output.
- `preview <pattern> <seconds> <palette> <frame-ms> <stride> <speed> <uptime-hours>`: also try negative speed and a long
  uptime (e.g. 60) for anything time-based.
- For memory/overflow bugs, build the renderer with `-fsanitize=address,undefined` (see the g++ line in `build.sh`) and
  run all patterns.
- CI (`.github/workflows/build.yml`) builds both boards and the beatsense variant, checks `geometry.h`, and renders every
  pattern. `.github/workflows/wokwi.yml` boots the Atom Lite build in Wokwi when the `WOKWI_CLI_TOKEN` secret is set (it
  isn't yet: the owner can't set secrets from the iOS app). Wokwi has no part for the encoder.
- When patterns change, refresh the public preview with `publish.sh` and propose the Pages change on a branch of
  costyn/costyn.github.io (it is the owner's live site; don't push its `main` without asking).

## Code layout and conventions

- `src/main.cpp` owns the runtime: TaskScheduler tasks, render loop, the two-buffer crossfade, brightness, palettes.
  `<TaskScheduler.h>` (the implementation) is included **only** there; everything else includes
  `<TaskSchedulerDeclarations.h>` via `globals.h`.
- Boards: `src/boards/atom_s3.h` and `atom.h` hold pins (`DATA_PIN`, I2C) and the LED power budget; the PlatformIO env
  sets `UZUMARU_BOARD_ATOM_S3` or `UZUMARU_BOARD_ATOM` and `config.h` includes the matching header. It must be included
  after `FastLED.h` (FastLED uses `DATA_PIN` as a template parameter name), so never force-include it with `-include`.
  Code must run on both the ESP32 and the ESP32-S3: no hard-coded GPIOs outside `boards/`.
- `src/config.h`: tuning constants. `src/globals.h`: shared state and the `Pattern {name, render}` type.
- Patterns are `void Name(CRGB *ledBuffer)` and must draw into the buffer they are given, never into `leds[]` or a
  specific buffer: two patterns render at once while crossfading. Register them in `src/patterns/playlist.cpp` with
  `PATTERN(Name)` and declare them in `src/patterns/patterns.h`.
  - Buffers have `NUM_LEDS + 1` entries; the lookup tables return `NUM_LEDS` for "no LED here", so that write is harmless.
  - Stateful patterns reset with `if (patternNeedsInit(ledBuffer)) { ... }`.
  - Time: use the speed-scaled clock, never `millis()` (unless the pattern deliberately ignores the speed knob).
    `animTime16(k)` for 16-bit phase math, `phase16(hz)` (shared helpers `phase16`, `forwardMs`, `fadeByTime` are
    declared in `patterns.h`), `fmod(g_timeAccumulator * k, 2 * PI)` for
    `sin()`. The clock can run backwards (negative speed); particles use `forwardMs()`.
  - Prefer elapsed-time fades (`fadeByTime()`) over per-frame `fadeToBlackBy`: the frame rate is not fixed.
- `src/patterns/ldirko.cpp` is ported from ldirko's firmware; `mapped.cpp` is the owner's patterns on ldirko's maps;
  `spiral.cpp` uses the exact geometry; `fibonacci256.cpp` holds ports from Jason Coon's Fibonacci256 firmware (GPL-3.0). New geometry-based patterns go in `spiral.cpp` (or a new file listed in
  `patterns.h`'s header comment).
- `src/geometry.h` (generated): per physical LED `ledX`, `ledY` (0-255, y down, centre 128), `ledAngle` (0 = up, clockwise),
  `ledRadius`, `ledRank` (seed order from the centre). Spiral arms: `rank % F` is the arm of family F (8, 13, 21, 34),
  `rank / F` the position along it, `(rank * q) % F` the arm's order round the disc (q = 3, 5, 8, 13 for F = 8, 13, 21,
  34). The 21 arms wind clockwise outward, the 34 counter-clockwise; the PCB is wired along the 21 arms.
- `src/audio.h`: `g_audio` is a beatsense frame (tempo, beat phase, levels). Without a board it is a synthetic beat set by
  the speed knob. `-D UZUMARU_BEATSENSE` polls a real board at I2C 0x42 on the encoder's bus (untested on hardware).
  `lib/beatsense_protocol` is copied from github.com/costyn/beatsense; update it by copying, don't edit.
- Match the surrounding style: Allman braces, 4-space indent in pattern files (2 in `main.cpp`), short comments that say
  why. Keep FastLED integer math (`sin8`, `scale8`, `qadd8`) in per-LED loops; avoid `double` there (software-emulated
  on the ESP32).

## Pattern design guidelines (the owner's taste, from watching them on the lamp)

The panel has only 256 LEDs behind a diffuser, and the owner judges patterns at the power-on defaults (brightness 100,
raised from 50 in late October 2026; speed 0.08). What worked and what didn't, October 2026:

- **Less is more.** A few strong elements beat many small ones: 3-4 thin spiral arms rather than 10 overlapping bands
  (Parastichy), runners on half the arms (SeedRunners). Fine detail is lost at this resolution: a pattern that breaks
  into many independent moving bits looks like noise (WLED's DriftRose did; ours stays one coherent figure).
- **Vary brightness, don't shove colours around.** The favourites keep colours roughly in place, or drifting slowly,
  and move *light* through them: pulses, glints, waves of brightness over a calm palette (Uzumaki, Bloom's glint,
  Octopus, pridePalette). Fast-moving colour fields read as busy. In the owner's words about pridePalette: the
  brighter LEDs look as if they move towards you in space, as if each sat on a little actuator. That depth effect,
  from brightness alone, is the thing to aim for.
- **Bright enough at the default brightness.** Check with `tools/preview/brightness.py` (average LED output, 0-255): full-field
  patterns should land around 30-45; Pacifica (18) and PolarLights (20) were too dark at brightness 50. Sparse patterns can sit lower if
  their features are bright. Don't judge brightness from the web preview: a screen can't match the LEDs (the owner:
  even full brightness on the web doesn't match the lamp at 50), so its slider starts at full; use the numbers.
- **Palettes with dark stretches** (about half the set: Vintage, Semi Blue, Yelblu Hot...) suit patterns where dark is
  the background. A pattern that spreads a whole palette over the disc (Bloom) should skip them.
- **Smooth at the default speed.** Slow gradients on few LEDs step visibly, and FastLED's dithering flickers at low
  brightness. Use 16-bit palette positions (`paletteColor16()`) for slow-moving gradients, and pick rates that look
  smooth at speed 0.08 (ColorWheel needed 1.6x its first rates).
- **Loops stay calm.** Strobing, high-contrast pulses (Heartbeat) are too intense for the playlist; keep them for
  one-shot music reactions.
- Shown as liked without changes: Uzumaki, Ripples, Vortex, Emitter, SwirlFibonacci, PolarNoise, Octopus, pridePalette.

## Dependencies

The owner prefers current versions. `platformio.ini` tracks the latest releases; pin a dependency only when a release
actually breaks the build or behaviour, with a comment saying why and what would unpin it. CI runs weekly to catch
upstream breakage.

## Attribution and licenses

GPL-3.0 firmware (inherited from ldirko's). Keep the original author headers in `ldirko.cpp`, `tables.h` and the palette
files, and say what was modified. The PCB design is CC BY-SA 4.0 (Uri Shaked / Wokwi). Update `CREDITS.md` when adding
third-party code, palettes or assets.

## Photos

Photos come from the owner's phone and can carry the GPS location of their home. Strip the EXIF GPS block (losslessly,
e.g. `piexif`, or `exiftool -gps:all=`) before committing any image here or to costyn.github.io. CI fails on GPS data
in this repo.

## Working style

- Use subagents for self-contained work (broad searches, dependency upgrades, research, tool setup), with a cheaper model
  where the task allows, to save tokens. Give them exact paths and tell them which files they may touch.
- Develop on the session's feature branch; don't push to `main` and don't open PRs unless asked.
- Commit messages: a short imperative subject, then what changed and why, and what was verified (and how).
