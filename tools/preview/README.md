# Pattern preview

Renders the firmware's patterns on your computer. The pattern sources are compiled unchanged against the real FastLED (its
stub platform) with a simulated clock, so what you see is what the code computes; only the LEDs and the diffuser are
approximated. The page can show the lamp as built (LEDs under a 0.4 mm PLA diffuser and a black overlay with one window
per LED, the default) or the bare LEDs; the toggle is under the lamp.

```
pio run                                        # once: downloads the current FastLED into .pio/libdeps
tools/preview/build.sh                         # builds .pio/preview/preview

python3 tools/preview/make_preview.py html preview.html            # every pattern, animated
python3 tools/preview/make_preview.py html preview.html Uzumaki     # just some
python3 tools/preview/make_preview.py png  still.png Bloom 1 3 6    # stills at 1, 3 and 6 s
tools/preview/publish.sh ../costyn.github.io                       # refresh costyn.github.io/uzumaru/preview/
```

Environment variables: `PALETTE` (palette index, default 7 = Sunset), `SPEED` (default 0.08), `PREVIEW_SECONDS` (clip
length, default 10).

The renderer itself writes raw frames (256 x RGB per frame) to stdout:

```
.pio/preview/preview --list
.pio/preview/preview <pattern> [seconds] [palette] [frame-ms] [stride] [speed] [uptime-hours]
```

`uptime-hours` starts the animation clock as if the lamp had been on that long, to check long-running behaviour.

It renders a single buffer, so it does not show crossfades. Files: `gen_overlay.py` and `overlay.json` (below), `host_main.cpp` (the driver), `host_globals.inc` (the
globals `main.cpp` normally owns), `stubs/` (just enough Arduino and TaskScheduler), `player.html` (page template).

## The overlay

`overlay.json` holds the overlay's windows in LED coordinates, one path per LED, taken from `extras/2d images/splendida_fix_svg.svg`
and registered to `src/geometry.h` (a similarity fit between the window centroids and `ledX`/`ledY`, then a one-to-one
assignment; the script prints the transform and the residuals). `make_preview.py` embeds it in the page. Regenerate it
only when the SVG or `geometry.h` changes: `pip install svgpathtools scipy`, then `python3 tools/preview/gen_overlay.py`.
Without `overlay.json` the page shows bare LEDs only.
