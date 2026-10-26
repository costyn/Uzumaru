#!/usr/bin/env python3
"""Average LED output of each pattern (0-255, before the lamp's brightness
setting), averaged over a few palettes. Full-field patterns the owner liked on
the lamp sit around 30-45; below ~25 they read as too dark at the default
brightness. Sparse patterns (comets, ripples) are lower by nature.

  python3 tools/preview/brightness.py [pattern ...]
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BIN = os.environ.get("PREVIEW_BIN", str(ROOT / ".pio" / "preview" / "preview"))
PALETTES = (7, 0, 30, 43)


def mean_output(pattern):
    total = count = 0
    for palette in PALETTES:
        raw = subprocess.run([BIN, pattern, "12", str(palette), "12", "6"], check=True, capture_output=True).stdout
        total += sum(raw)
        count += len(raw)
    return total / count


names = sys.argv[1:] or [l for l in subprocess.run([BIN, "--list"], check=True, capture_output=True,
                                                    text=True).stdout.splitlines() if l and not l.startswith("#")]
for value, name in sorted((mean_output(n), n) for n in names):
    print(f"{value:6.1f}  {name}")
