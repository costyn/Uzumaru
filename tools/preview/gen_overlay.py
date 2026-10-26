#!/usr/bin/env python3
"""Generate tools/preview/overlay.json: the lamp's black overlay, registered to the LEDs.

The overlay is extras/2d images/splendida_fix_svg.svg: one compound path whose subpaths are the disc outline, one
window (cell) per LED, the centre dot and three small alignment holes. This script

  1. parses the path and finds the 256 cells (area ~400-580 mm^2; the outline, the centre dot and the alignment
     holes are told apart by size),
  2. fits a similarity transform (scale, rotation, translation, optional mirror) from the cell centroids to the
     LED positions ledX/ledY of src/geometry.h (0-255, y down), by trying every rotation, then iterating
     nearest-neighbour matching and least squares (Umeyama),
  3. assigns each cell to its LED with the Hungarian algorithm and checks the assignment,
  4. writes the cells, the outline and the centre dot, transformed into LED coordinates, as SVG path strings in
     1/10 LED units (integers, relative, so the file stays small).

Run it only when the SVG or src/geometry.h changes; overlay.json is committed. Needs numpy, scipy and svgpathtools
(pip install svgpathtools scipy).

  python3 tools/preview/gen_overlay.py            # writes tools/preview/overlay.json and prints the fit
"""
import json
import re
import sys
from pathlib import Path

import numpy as np
from scipy.optimize import linear_sum_assignment
from svgpathtools import parse_path

ROOT = Path(__file__).resolve().parents[2]
SVG = ROOT / "extras" / "2d images" / "splendida_fix_svg.svg"
OUT = Path(__file__).with_name("overlay.json")
Q = 10  # output units per LED unit


def geometry():
    src = (ROOT / "src" / "geometry.h").read_text()
    def tab(name):
        body = re.search(r"\b" + name + r"\[NUM_LEDS\][^{]*\{(.*?)\};", src, re.S).group(1)
        return [int(v) for v in re.findall(r"\d+", body)]
    return np.array([tab("ledX"), tab("ledY")], float).T


def polygon(sp, per=24):
    pts = []
    for seg in sp:
        for t in np.linspace(0, 1, per, endpoint=False):
            pts.append(seg.point(t))
    z = np.array(pts)
    return np.stack([z.real, z.imag], 1)


def centroid_area(poly):
    x, y = poly[:, 0], poly[:, 1]
    x1, y1 = np.roll(x, -1), np.roll(y, -1)
    cr = x * y1 - x1 * y
    a = cr.sum() / 2
    return np.array([((x + x1) * cr).sum(), ((y + y1) * cr).sum()]) / (6 * a), abs(a)


def umeyama(src, dst, mirror):
    """Least-squares similarity: dst ~ s * R @ (M @ src) + t, M = diag(-1, 1) when mirrored."""
    m = np.diag([-1.0, 1.0]) if mirror else np.eye(2)
    a = src @ m
    ma, mb = a.mean(0), dst.mean(0)
    u, d, vt = np.linalg.svd((dst - mb).T @ (a - ma))
    sgn = np.sign(np.linalg.det(u @ vt))
    r = u @ np.diag([1, sgn]) @ vt
    s = (d * [1, sgn]).sum() / ((a - ma) ** 2).sum()
    return s * r @ m, mb - s * r @ m @ ma  # A (2x2) and t: dst = A @ p + t


def main():
    d = re.search(r'<path d="([^"]*)"', SVG.read_text(encoding="utf-8")).group(1)
    subs = parse_path(d).continuous_subpaths()
    polys = [polygon(sp) for sp in subs]
    info = [centroid_area(p) for p in polys]
    areas = np.array([a for _, a in info])
    outline = int(areas.argmax())
    cells = [i for i in range(len(subs)) if i != outline and 380 < areas[i] < 700]
    small = [i for i in range(len(subs)) if i != outline and i not in cells]
    # the centre dot is the small piece nearest the disc centre; the others are alignment holes
    dot = min(small, key=lambda i: np.hypot(*(info[i][0] - info[outline][0])))
    print(f"{len(subs)} subpaths: outline, {len(cells)} cells, centre dot, {len(small) - 1} alignment holes", file=sys.stderr)
    assert len(cells) == 256, len(cells)

    cen = np.array([info[i][0] for i in cells])
    leds = geometry()
    best = None
    for mirror in (False, True):
        for deg in range(0, 360, 2):
            th = np.radians(deg)
            r = np.array([[np.cos(th), -np.sin(th)], [np.sin(th), np.cos(th)]])
            m = np.diag([-1.0, 1.0]) if mirror else np.eye(2)
            s = leds.std() / cen.std()
            a = s * r @ m
            t = leds.mean(0) - a @ cen.mean(0)
            for _ in range(6):  # ICP
                mapped = cen @ a.T + t
                dist = np.linalg.norm(mapped[:, None] - leds[None], axis=2)
                j = dist.argmin(1)
                a, t = umeyama(cen, leds[j], mirror)
            mapped = cen @ a.T + t
            dist = np.linalg.norm(mapped[:, None] - leds[None], axis=2)
            r_i, c_i = linear_sum_assignment(dist)
            err = dist[r_i, c_i].mean()
            if best is None or err < best[0]:
                best = (err, mirror, deg, a, t)
    err, mirror, deg, a, t = best
    # final refinement with the Hungarian assignment
    for _ in range(10):
        mapped = cen @ a.T + t
        dist = np.linalg.norm(mapped[:, None] - leds[None], axis=2)
        ci, li = linear_sum_assignment(dist)
        a, t = umeyama(cen[ci], leds[li], mirror)
    mapped = cen @ a.T + t
    dist = np.linalg.norm(mapped[:, None] - leds[None], axis=2)
    ci, li = linear_sum_assignment(dist)
    res = dist[ci, li]
    nn = dist.argmin(1)
    scale = np.sqrt(abs(np.linalg.det(a)))
    rot = np.degrees(np.arctan2(a[1, 0], a[0, 0]))
    print(f"similarity: mirror={mirror} scale={scale:.4f} LED units per SVG unit, rotation={rot:.2f} deg, "
          f"translation=({t[0]:.2f}, {t[1]:.2f})", file=sys.stderr)
    print(f"residuals (LED units; 1 unit = 1/255 of the LED span, a cell is ~13 wide): mean {res.mean():.2f}, "
          f"rms {np.sqrt((res ** 2).mean()):.2f}, max {res.max():.2f}", file=sys.stderr)
    print(f"one-to-one: {len(set(li)) == 256}; Hungarian == nearest neighbour for {int((nn[ci] == li).sum())}/256 cells; "
          f"LED grid quantisation alone is ~0.3 rms", file=sys.stderr)
    # second-best distance gives the margin for the assignment
    srt = np.sort(dist, 1)
    print(f"nearest / second-nearest LED distance: worst ratio {np.max(srt[:, 0] / srt[:, 1]):.2f}", file=sys.stderr)

    def path_d(poly_idx):
        # transform to LED coordinates; sample the Beziers at 4 points per segment for a smooth outline
        sp = subs[poly_idx]
        pts = []
        for seg in sp:
            for tt in np.linspace(0, 1, 4, endpoint=False):
                pts.append(seg.point(tt))
        z = np.array(pts)
        p = np.stack([z.real, z.imag], 1) @ a.T + t
        q = np.round(p * Q).astype(int)
        keep = [0] + [k for k in range(1, len(q)) if (q[k] != q[k - 1]).any()]
        q = q[keep]
        out = f"M{q[0][0]} {q[0][1]}"
        prev = q[0]
        for pt in q[1:]:
            dx, dy = pt - prev
            out += f"l{dx} {dy}" if dy >= 0 else f"l{dx}{dy}"
            prev = pt
        return out.replace(" -", "-") + "z"

    order = np.empty(256, int)  # order[led] = cell
    order[li] = ci
    data = {
        "q": Q,
        "outline": path_d(outline),
        "dot": path_d(dot),
        "cells": [path_d(cells[order[led]]) for led in range(256)],
    }
    c = (centroid_area(polygon(subs[dot]))[0] @ a.T + t)
    r = np.sqrt(centroid_area(polygon(subs[dot]))[1] / np.pi) * scale
    data["dot_c"] = [round(float(c[0]), 2), round(float(c[1]), 2), round(float(r), 2)]
    oc = centroid_area(polygon(subs[outline]))[0] @ a.T + t
    data["outline_c"] = [round(float(oc[0]), 2), round(float(oc[1]), 2)]
    OUT.write_text(json.dumps(data, separators=(",", ":")))
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)", file=sys.stderr)


if __name__ == "__main__":
    main()
