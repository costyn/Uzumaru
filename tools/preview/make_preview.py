#!/usr/bin/env python3
"""Render pattern previews from the desktop renderer.

  make_preview.py html OUT.html [pattern ...]    animated HTML player
  make_preview.py png  OUT.png  pattern [t ...]  still frames (seconds)

Options via environment: PREVIEW_BIN (renderer path), PREVIEW_SECONDS, PALETTE,
SPEED, PREVIEW_FRAGMENT=1 (leave out <!doctype>/<head>, for pages that get
wrapped by their host, such as a claude.ai artifact), PREVIEW_SPLIT=1 (write
each clip to clips/<name>.dat next to the page, fetched when it is played,
instead of embedding all of them; for web hosting). LED positions come from
src/geometry.h.
"""
import base64
import gzip
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BIN = os.environ.get("PREVIEW_BIN", str(ROOT / ".pio" / "preview" / "preview"))
SECONDS = float(os.environ.get("PREVIEW_SECONDS", "20"))
# Clip lengths that show a pattern's whole cycle (seconds at the default speed).
# Bloom's grow-flower-shed cycle is exactly 16 s, so its clip loops seamlessly.
CLIP_SECONDS = {"Bloom": 16, "Parastichy": 29, "Uzumaki": 30, "ColorWheel": 30, "hypnoticWings": 25}
PALETTE = os.environ.get("PALETTE", "7")
SPEED = os.environ.get("SPEED", "0.08")
FRAME_MS, STRIDE = 12, 3  # simulate ~83 fps, keep every 3rd frame (~28 fps)
N = 256


def geometry():
    src = (ROOT / "src" / "geometry.h").read_text()
    def tab(name):
        body = re.search(r"\b" + name + r"\[NUM_LEDS\][^{]*\{(.*?)\};", src, re.S).group(1)
        return [int(v) for v in re.findall(r"\d+", body)]
    return tab("ledX"), tab("ledY")


def render(pattern, seconds=SECONDS):
    raw = subprocess.run(
        [BIN, pattern, str(seconds), PALETTE, str(FRAME_MS), str(STRIDE), SPEED],
        check=True, capture_output=True).stdout
    return raw


def patterns():
    out = subprocess.run([BIN, "--list"], check=True, capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if l and not l.startswith("#")]


def to_display(v):
    # LEDs emit linear light; screens expect sRGB. Approximate the look.
    return int(round(255 * (v / 255) ** (1 / 2.2)))


def png(out, pattern, times):
    from PIL import Image, ImageDraw, ImageFilter
    xs, ys = geometry()
    raw = render(pattern, max(times) + 0.1)
    size, pad = 360, 20
    sheet = Image.new("RGB", (size * len(times), size), (8, 8, 10))
    lut = [to_display(v) for v in range(256)]
    for k, t in enumerate(times):
        f = int(t * 1000 / FRAME_MS / STRIDE)
        frame = raw[f * N * 3:(f + 1) * N * 3]
        img = Image.new("RGB", (size, size), (8, 8, 10))
        d = ImageDraw.Draw(img)
        s = (size - 2 * pad) / 255
        for i in range(N):
            r, g, b = (lut[c] for c in frame[i * 3:i * 3 + 3])
            cx, cy = pad + xs[i] * s, pad + ys[i] * s
            d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], fill=(r, g, b))
        sheet.paste(img.filter(ImageFilter.GaussianBlur(1.2)), (k * size, 0))
    sheet.save(out)


def html(out, names):
    xs, ys = geometry()
    clips = []
    for name in names:
        raw = render(name, CLIP_SECONDS.get(name, SECONDS) if "PREVIEW_SECONDS" not in os.environ else SECONDS)
        packed = gzip.compress(raw, 9, mtime=0)
        if os.environ.get("PREVIEW_SPLIT") == "1":
            # .dat, not .gz: some servers would add Content-Encoding and unpack it early
            clip_dir = Path(out).parent / "clips"
            clip_dir.mkdir(parents=True, exist_ok=True)
            (clip_dir / f"{name}.dat").write_bytes(packed)
            clips.append({"name": name, "url": f"clips/{name}.dat"})
        else:
            clips.append({"name": name, "data": base64.b64encode(packed).decode()})
        print(f"{name}: {len(raw) // (N * 3)} frames", file=sys.stderr)
    template = (Path(__file__).parent / "player.html").read_text()
    # the black overlay with its windows, registered to the LEDs (gen_overlay.py); without it only bare LEDs are shown
    overlay_file = Path(__file__).with_name("overlay.json")
    overlay = overlay_file.read_text().strip() if overlay_file.exists() else "null"
    page = template.replace("/*GEOMETRY*/null", json.dumps({"x": xs, "y": ys})) \
                   .replace("/*OVERLAY*/null", overlay) \
                   .replace("/*CLIPS*/[]", json.dumps(clips)) \
                   .replace("/*FPS*/28", str(round(1000 / (FRAME_MS * STRIDE))))
    if os.environ.get("PREVIEW_FRAGMENT") != "1":
        page = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
                '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
                + page.replace("<main>", "</head>\n<body>\n<main>", 1) + "\n</body>\n</html>\n")
    Path(out).write_text(page, encoding="utf-8")


if __name__ == "__main__":
    mode, out, *rest = sys.argv[1:]
    if mode == "png":
        png(out, rest[0], [float(t) for t in rest[1:]] or [1, 3, 5])
    else:
        html(out, rest or patterns())
