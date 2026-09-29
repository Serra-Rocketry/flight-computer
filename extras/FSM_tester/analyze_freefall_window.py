#!/usr/bin/env python3
"""Analyze real flight data to size an FSM-independent free-fall detector.

Reads the same datasets as the FSM validators and reports:
  1. Total accel profile per flight phase (burn, coast, descent pre/post deploy)
  2. Duration of sustained near-zero-g windows (free-fall candidates)
  3. Altitude during those windows (to check the ground guard)
  4. Max total accel on the ground (vibration baseline) — false positive check

Usage: python3 analyze_freefall_window.py
"""
import csv
import math
import sys
from collections import defaultdict

DATASETS = {
    "simulado (RocketPy ~950m)": "dados_simulados.csv",
    "real (~272m)": "dados_filtrados.csv",
}


def load(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            rows.append(r)
    return rows


def total_accel(ax, ay, az):
    return math.sqrt(ax * ax + ay * ay + az * az)


def analyze(path, header_cols):
    rows = load(path)
    # NOTE: "millis" column is actually in SECONDS in these CSVs (validator uses it directly)
    t0 = float(rows[0]["millis"])
    prev_t = t0
    alt0 = float(rows[0]["altp"])

    phases = []          # (t_start, t_end, label)
    zero_g_windows = []  # (t_start, t_end, h_min, accel_max)
    ground_spikes = []
    in_zero = False
    win_start = 0.0
    win_hmin = 1e9
    win_accmax = 0.0

    # Rough phase segmentation via vz sign / accel magnitude
    prev_vz = 0.0
    prev_h = 0.0
    burn_until = 0.0
    apogee_t = None

    prev_alt = float(rows[0]["altp"])
    for i, r in enumerate(rows):
        t = float(r["millis"])
        dt = t - prev_t
        if dt <= 0:
            dt = 1e-3
        h = float(r["altp"]) - alt0
        ax, ay, az = (float(r[c]) for c in ("ax", "ay", "az"))
        acc = total_accel(ax, ay, az)
        vz = (h - prev_h) / dt if dt > 0 else 0.0
        prev_t, prev_h = t, h

        # ground baseline (first 1s, before launch)
        if t < 1.0:
            ground_spikes.append(acc)
            continue

        # zero-g window tracking
        if acc < 3.0 and h > 20.0:
            if not in_zero:
                in_zero = True
                win_start = t
                win_hmin = h
                win_accmax = acc
            else:
                win_hmin = min(win_hmin, h)
                win_accmax = max(win_accmax, acc)
        else:
            if in_zero:
                zero_g_windows.append((win_start, t, win_hmin, win_accmax))
                in_zero = False

        if vz > 2.0 and acc > 4.0:
            burn_until = t
        if apogee_t is None and vz < 0 and prev_vz >= 0:
            apogee_t = t
        prev_vz = vz

    if in_zero:
        zero_g_windows.append((win_start, prev_t, win_hmin, win_accmax))

    # filter windows: only those at least 0.3s (free-fall needs sustained)
    sustained = [w for w in zero_g_windows if w[1] - w[0] >= 0.3]

    print(f"\n=== {path} ===")
    print(f"samples={len(rows)}  dur={prev_t:.1f}s  burn_until~{burn_until:.2f}s  apogee~{apogee_t}")
    if ground_spikes:
        print(f"GROUND baseline (t<1s): max accel = {max(ground_spikes):.2f} m/s²")
    print(f"zero-g (<3 m/s², h>20m) windows total: {len(zero_g_windows)}, sustained >=0.3s: {len(sustained)}")
    for w in sustained[:12]:
        print(f"  [{w[0]:7.2f}s -> {w[1]:7.2f}s] dur={w[1]-w[0]:5.2f}s  "
              f"h_min={w[2]:6.1f}m  acc_max={w[3]:4.2f} m/s²")
    # where does the LAST sustained window start relative to apogee?
    if sustained and apogee_t:
        last = sustained[-1]
        print(f"  last sustained window starts {last[0]-apogee_t:+.2f}s relative to apogee")


for name, path in DATASETS.items():
    try:
        analyze(path, None)
    except Exception as e:
        print(f"{name}: ERROR {e}")
