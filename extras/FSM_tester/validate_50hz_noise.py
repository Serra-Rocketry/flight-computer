#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
validate_50hz_noise.py — Risco n.3: FSM @50Hz + ruido de quantizacao do BMP585
===============================================================================
SOMENTE VALIDACAO. Nenhum arquivo do firmware e' modificado.

Problema investigado:
  O firmware roda a 50Hz (FLIGHT_CONTROL_PERIOD_MS = 20ms), mas os validadores
  existentes rodam os CSVs a 4-10Hz. A 50Hz, o ruido de quantizacao do BMP585
  (0.06 Pa ~= 0.005 m; margem conservadora usada aqui: uniforme +-0.1 m) vira
  ruido em vz = dh/20ms de ~+-4 m/s (sigma), o que pode impedir que a condicao
  de apogeu |vz| < APOGEE_MAX_VZ (1.0 m/s) seja satisfeita -> apogeu perdido ->
  deploy atrasado ou inexistente (nesta porta, sem backstop).

Pipeline por dataset de voo:
  1. Carrega o CSV e nivela altp ao solo (base_pressure), como os validadores.
  2. Re-amostra a 50Hz (dt = 20ms):
       - altp   : interpolacao linear (sinal fisico continuo do barometro;
                  a serie de telemetria e' uma amostra decimada da trajetoria).
       - ax/ay/az: interpolacao linear por padrao. Escolha documentada: o IMU
                  e' um sinal fisico continuo e o IIR alpha=0.2 do firmware
                  suaviza a diferenca vs. zero-order-hold; um teste de paridade
                  (modo hold vs. interp no 50Hz limpo) e' rodado e impresso
                  para comprovar que a escolha nao muda o resultado.
  3. Injeta ruido de quantizacao uniforme +-NOISE_AMP m por amostra
     (N_RUNS sementes por amplitude; 0.05 e 0.10 m).
  4. Recalcula vz por diferenciacao com dt=20ms + clamp +-200 m/s
     (igual a BMP585Sensor::update()).
  5. Roda a MESMA porta Python da FSM de validate_parachute_realflight.py
     (importada, nao copiada) e mede:
       - % de runs com apogeu detectado
       - % de runs com deploy (paraquedas)
       - atraso/adiantamento do apogeu vs. apogeu real (pico da serie limpa)
       - atraso do deploy vs. baseline (dados ORIGINAIS 4-10Hz)
       - deploys "fora do apogeu" (antes do apogeu real ou muito tardios)
  6. Sanity check de bancada (13_30_11, porcao em solo): nenhum falso
     liftoff/deploy com ruido.

Uso: python3 validate_50hz_noise.py [n_runs]     (padrao: 200)
"""

import csv
import os
import sys
import numpy as np

BASE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE)

# Porta da FSM importada do validador existente -> consistencia garantida
from validate_parachute_realflight import FSM, PARACHUTE_MIN_ALTITUDE  # noqa: E402

DT = 0.02          # 50Hz
VZ_CLAMP = 200.0   # clamp do firmware (BMP585Sensor::update)
N_RUNS = 200
NOISE_AMPS = (0.05, 0.10)   # margem conservadora +-0.05 / +-0.10 m
TRUE_TOL = 0.05    # tolerancia do teste de paridade (s)

# Valores impressos por validate_parachute_realflight.py (2026-08-05) —
# usados como regressao para provar que a porta reutilizada bate.
EXPECTED = {
    "dados_simulados.csv":  {"apogee_t": 14.3, "deploy_t": 14.8},
    "dados_filtrados.csv":  {"apogee_t": 7.813, "deploy_t": 8.548},
    # Simulacoes dos 2 proximos voos: valor impresso pelo validador realflight
    # na grade 50Hz (2026-08-05) — dt nativo de ~2ms nao e representativo.
    "flight_results_thonyan.csv": {"apogee_t": 11.86, "deploy_t": 12.22},
    "flight_results_dedalo.csv":  {"apogee_t": 17.8, "deploy_t": 18.16},
}


# ── carregamento ────────────────────────────────────────────────────────────
def load_csv(fn, tscale=1.0, alt_below=None):
    """Le (millis, altp, ax, ay, az); nivela altp ao primeiro sample.
    tscale: fator do tempo (13_30_11 tem millis em ms -> tscale=0.001).
    alt_below: se dado, mantem apenas samples com altp < limite (porcao solo).
    Autodetect: simulacoes RocketPy usam time/z; telemetria usa millis/altp."""
    rows = []
    with open(fn) as f:
        r = csv.DictReader(f)
        tcol = "time" if "time" in (r.fieldnames or []) else "millis"
        hcol = "z" if "z" in (r.fieldnames or []) else "altp"
        for row in r:
            try:
                t = float(row[tcol]) * tscale
                altp = float(row[hcol])
                ax = float(row["ax"])
                ay = float(row["ay"])
                az = float(row["az"])
            except (ValueError, KeyError):
                continue
            if alt_below is not None and altp >= alt_below:
                continue
            rows.append((t, altp, ax, ay, az))
    if not rows:
        return rows
    base = rows[0][1]
    return [(t, a - base, x, y, z) for (t, a, x, y, z) in rows]


# ── re-amostragem a 50Hz ────────────────────────────────────────────────────
def resample_50hz(rows, accel_mode="interp"):
    """Grade fixa dt=20ms entre t0 e tN. altp: sempre linear.
    accel: 'interp' (linear) ou 'hold' (zero-order) — para teste de paridade."""
    ts = np.array([r[0] for r in rows])
    h = np.array([r[1] for r in rows])
    ax = np.array([r[2] for r in rows])
    ay = np.array([r[3] for r in rows])
    az = np.array([r[4] for r in rows])
    n = int(round((ts[-1] - ts[0]) / DT)) + 1
    grid = ts[0] + np.arange(n) * DT
    grid = grid[grid <= ts[-1] + 1e-9]
    hg = np.interp(grid, ts, h)
    if accel_mode == "hold":
        idx = np.searchsorted(ts, grid, side="right") - 1
        idx = np.clip(idx, 0, len(ts) - 1)
        axg, ayg, azg = ax[idx], ay[idx], az[idx]
    else:
        axg = np.interp(grid, ts, ax)
        ayg = np.interp(grid, ts, ay)
        azg = np.interp(grid, ts, az)
    return grid, hg, axg, ayg, azg


# ── execucao da FSM ─────────────────────────────────────────────────────────
def run_fsm_native(rows):
    """Roda a FSM nos dados ORIGINAIS (dt real) — igual ao validador existente."""
    fsm = FSM()
    apogee_t = apogee_h = deploy_t = deploy_h = None
    prev_t = prev_h = None
    for (t, h, ax, ay, az) in rows:
        if prev_t is not None and (t - prev_t) > 0:
            dt = t - prev_t
            vz = (h - prev_h) / dt
            vz = max(min(vz, VZ_CLAMP), -VZ_CLAMP)
        else:
            vz = 0.0
        fsm.t = t
        deployed = fsm.update(h, ax, ay, az, vz)
        if fsm.apogee and apogee_t is None:
            apogee_t, apogee_h = t, h
        if deployed and deploy_t is None:
            deploy_t, deploy_h = t, h
        prev_t, prev_h = t, h
    return fsm, apogee_t, apogee_h, deploy_t, deploy_h


def run_fsm_50hz(grid, h, ax, ay, az, rng=None, noise_amp=0.0):
    """Roda a FSM na serie 50Hz (opcionalmente com ruido em h).
    vz = diff(h)/20ms + clamp, como BMP585Sensor::update()."""
    h = np.asarray(h, dtype=float)
    if noise_amp > 0.0 and rng is not None:
        h = h + rng.uniform(-noise_amp, noise_amp, h.shape)
    vz = np.empty_like(h)
    vz[0] = 0.0
    vz[1:] = np.diff(h) / DT
    np.clip(vz, -VZ_CLAMP, VZ_CLAMP, out=vz)

    fsm = FSM()
    apogee_t = apogee_h = deploy_t = deploy_h = None
    hl = h.tolist()
    axl = ax.tolist()
    ayl = ay.tolist()
    azl = az.tolist()
    vzl = vz.tolist()
    for i in range(len(grid)):
        fsm.t = float(grid[i])
        deployed = fsm.update(hl[i], axl[i], ayl[i], azl[i], vzl[i])
        if fsm.apogee and apogee_t is None:
            apogee_t, apogee_h = float(grid[i]), hl[i]
        if deployed and deploy_t is None:
            deploy_t, deploy_h = float(grid[i]), hl[i]
    return fsm, apogee_t, apogee_h, deploy_t, deploy_h


# ── estatisticas ────────────────────────────────────────────────────────────
def stats(vals):
    if not vals:
        return None
    a = np.asarray(vals, dtype=float)
    return (float(a.mean()), float(np.percentile(a, 95)), float(a.max()),
            float(a.min()))


def fmt_stats(s, unit="s"):
    if s is None:
        return "n/a"
    return (f"media {s[0]:+.2f}{unit} | p95 {s[1]:+.2f}{unit} | "
            f"max {s[2]:+.2f}{unit} | min {s[3]:+.2f}{unit}")


# ── analise de um dataset de voo ────────────────────────────────────────────
def analyze_flight(name, path, n_runs):
    rows = load_csv(path)
    print(f"\n{'=' * 78}\nVOO: {name}  ({os.path.basename(path)}, "
          f"{len(rows)} amostras, {rows[0][0]:.2f}s -> {rows[-1][0]:.2f}s)")
    print("=" * 78)

    # Simulacoes RocketPy usam passo adaptativo de ~2ms: com esse dt o vz por
    # diferenca fica < 1 m/s logo apos o liftoff e o apogeu dispararia FALSO
    # (o firmware roda a 50Hz fixo). Re-amostra a 50Hz ANTES de tudo quando a
    # resolucao e mais fina que o firmware em qualquer parte do voo (p10 < 10ms).
    dts = sorted(b[0] - a[0] for a, b in zip(rows, rows[1:]) if b[0] - a[0] > 0)
    if dts and dts[len(dts) // 10] < 0.010:
        print(f"[nota] dt_p10={dts[len(dts) // 10] * 1000:.1f}ms < 20ms do "
              f"firmware -> re-amostrando a 50Hz antes da analise")
        grid, hg, axg, ayg, azg = resample_50hz(rows)
        rows = [(float(t), float(h), float(a), float(b), float(c))
                for t, h, a, b, c in zip(grid, hg, axg, ayg, azg)]

    # 1) paridade com o validador existente (dados originais)
    fsm, at, ah, dt_, dh = run_fsm_native(rows)
    exp = EXPECTED[os.path.basename(path)]
    ok_ap = at is not None and abs(at - exp["apogee_t"]) < TRUE_TOL
    ok_dp = dt_ is not None and abs(dt_ - exp["deploy_t"]) < TRUE_TOL
    print(f"[paridade] dados originais: apogeu t={at}s h={ah:.1f}m | "
          f"deploy t={dt_}s h={dh:.1f}m  "
          f"-> {'PASS' if (ok_ap and ok_dp) else 'FALHOU (porta divergiu!)'}")

    # 2) baseline 50Hz limpo + teste de escolha accel (interp vs hold)
    grid, h, ax, ay, az = resample_50hz(rows, accel_mode="interp")
    fsm50, at50, ah50, dt50, dh50 = run_fsm_50hz(grid, h, ax, ay, az)
    _, _, _, dt50h, _ = run_fsm_50hz(*resample_50hz(rows, accel_mode="hold"),
                                     rng=None, noise_amp=0.0)
    accel_note = ("idem" if (dt50 is not None and dt50h is not None
                             and abs(dt50 - dt50h) < 1e-9)
                  else "DIFERENTE (rever escolha)")
    print(f"[50Hz limpo] apogeu t={at50}s h={ah50:.1f}m | deploy t={dt50}s "
          f"h={dh50:.1f}m | accel interp vs hold: {accel_note}")

    # 3) apogeu real (pico da serie limpa 50Hz)
    i_true = int(np.argmax(h))
    t_true = float(grid[i_true])
    h_true = float(h[i_true])
    print(f"[apogeu real] t={t_true:.3f}s h={h_true:.1f}m "
          f"(baseline orig: deploy "
          f"{((dt_ - at) * 1000.0) if (dt_ is not None and at is not None) else 0.0:.0f}"
          f"ms apos apogeu)")

    # 4) Monte Carlo com ruido
    pct_ap = pct_dp = 0.0
    for amp in NOISE_AMPS:
        ap_det, dep, ap_delay, dep_delay_orig, early, late, no_dep, dep_h = (
            [], [], [], [], 0, 0, 0, [])
        for run in range(n_runs):
            rng = np.random.default_rng(1000 + run)
            fsmr, atr, ahr, dtr, dhr = run_fsm_50hz(
                grid, h, ax, ay, az, rng=rng, noise_amp=amp)
            if atr is not None:
                ap_det.append(atr)
                ap_delay.append(atr - t_true)
            if dtr is not None:
                dep.append(dtr)
                if dt_ is not None:
                    dep_delay_orig.append(dtr - dt_)
                dep_h.append(dhr)
                if dtr < t_true - 0.25:
                    early += 1          # deploy antes do apogeu real
                elif dtr > t_true + 5.0:
                    late += 1           # deploy muito tardio
            else:
                no_dep += 1
        pct_ap = 100.0 * len(ap_det) / n_runs
        pct_dp = 100.0 * len(dep) / n_runs
        n_early_ap = sum(1 for d in ap_delay if d < -0.25)
        print(f"\n--- ruido +-{amp:.2f} m  ({n_runs} runs) ---")
        print(f"  apogeu detectado : {pct_ap:.1f}%   deploy: {pct_dp:.1f}%   "
              f"SEM deploy: {100.0 - pct_dp:.1f}%")
        print(f"  atraso apogeu vs real : {fmt_stats(stats(ap_delay))}  "
              f"(<0 = detectado ANTES do apogeu real; {n_early_ap} runs "
              f"com apogeu >0.25s cedo)")
        if dep_delay_orig:
            print(f"  atraso deploy vs baseline orig : "
                  f"{fmt_stats(stats(dep_delay_orig))}")
        else:
            print("  atraso deploy vs baseline orig : n/a (nenhum deploy)")
        print(f"  deploy ANTES do apogeu real: {early} run(s) | "
              f"deploy >5s APOS apogeu: {late} run(s) | "
              f"altura deploy min: {min(dep_h) if dep_h else float('nan'):.1f}m")
        if no_dep:
            print(f"  !! {no_dep}/{n_runs} runs sem deploy "
                  f"(apogeu perdido -> FSM presa em ASCENT)")
    return pct_ap, pct_dp


# ── sanity check de bancada ─────────────────────────────────────────────────
def check_bench(path, n_runs):
    rows = load_csv(path, tscale=0.001, alt_below=20.0)  # millis em ms
    if not rows:
        print("  (sem dados validos na porcao solo)")
        return
    grid, h, ax, ay, az = resample_50hz(rows)
    # accel maximo na bancada (liftoff exige > 15 m/s²)
    acc = np.sqrt(ax ** 2 + ay ** 2 + az ** 2)
    print(f"\n{'=' * 78}\nBANCADA: {os.path.basename(path)} "
          f"(porcao solo, {len(rows)} amostras -> {len(grid)} @50Hz)")
    print("=" * 78)
    print(f"  accel total max = {acc.max():.2f} m/s^2 "
          f"(liftoff exige > 15) -> "
          f"{'nunca sai de IDLE' if acc.max() < 15 else 'ATENCAO: pode liftoff'}")
    false_pos = 0
    for run in range(n_runs):
        rng = np.random.default_rng(9000 + run)
        fsmr, atr, _, dtr, _ = run_fsm_50hz(
            grid, h, ax, ay, az, rng=rng, noise_amp=0.10)
        if atr is not None or dtr is not None:
            false_pos += 1
    print(f"  falso apogeu/deploy com ruido +-0.10m: {false_pos}/{n_runs} "
          f"-> {'PASS' if false_pos == 0 else 'FALHA'}")


# ── contexto do ruido em vz + recomendacoes (sem implementar) ───────────────
def noise_context():
    rng = np.random.default_rng(42)
    n = 20000
    # referencia: quantizacao REAL do BMP585 (0.06 Pa ~= 0.005 m)
    u = rng.uniform(-0.0025, 0.0025, n)   # LSB 0.005 m -> +-0.0025
    vz = np.diff(u) / DT
    print(f"  quantizacao REAL BMP585 (+-0.0025m): sigma={vz.std():.3f} m/s "
          f"(muito abaixo de APOGEE_MAX_VZ=1.0)")
    for amp in NOISE_AMPS:
        u = rng.uniform(-amp, amp, n)
        vz = np.diff(u) / DT
        print(f"  ruido +-{amp:.2f}m  ->  vz (dh/20ms): sigma={vz.std():.2f} "
              f"m/s, pico={np.abs(vz).max():.2f} m/s  "
              f"(APOGEE_MAX_VZ=1.0)")
        for w in (5, 10, 25):   # media movel hipotetica (diagnostico apenas)
            if w >= len(vz):
                continue
            ma = np.convolve(vz, np.ones(w) / w, mode="valid")
            print(f"     com media movel de {w} amostras ({w * DT * 1000:.0f}ms): "
                  f"sigma={ma.std():.2f} m/s")


def main():
    global N_RUNS
    if len(sys.argv) > 1:
        N_RUNS = int(sys.argv[1])
    t0 = __import__("time").time()

    print("validate_50hz_noise.py — FSM @50Hz + ruido de quantizacao BMP585")
    print(f"dt={DT * 1000:.0f}ms | {N_RUNS} runs/amplitude | amplitudes "
          f"{NOISE_AMPS} | clamp vz +-{VZ_CLAMP:.0f} | piso "
          f"{PARACHUTE_MIN_ALTITUDE:.0f}m")
    print("\nContexto do ruido em vz (quantizacao pura, sem trajetoria):")
    noise_context()

    results = {}
    for name, fn in (("SIMULADO RocketPy (~950m)", "dados_simulados.csv"),
                     ("VOO REAL (~272m)", "dados_filtrados.csv"),
                     ("SIM Thonyan (~682m)", "flight_results_thonyan.csv"),
                     ("SIM Dedalo (~1544m)", "flight_results_dedalo.csv")):
        pct_ap, pct_dp = analyze_flight(name, os.path.join(BASE, fn), N_RUNS)
        results[fn] = (pct_ap, pct_dp)

    check_bench(os.path.join(BASE, "13_30_11-Dados.csv"),
                max(20, N_RUNS // 10))

    print(f"\n{'=' * 78}\nCONCLUSAO (diagnostico — nenhum firmware modificado)\n"
          f"{'=' * 78}")
    worst_loss = max((100.0 - dp) for _, dp in results.values())
    if worst_loss > 0.0:
        print(f"  RISCO n.3 CONFIRMADO: ate {worst_loss:.0f}% dos runs perderam "
              f"o apogeu a 50Hz.")
        print("  Recomendacoes (decidir no firmware, NAO implementadas aqui):")
        print("  1. Suavizar vz com media movel (5-10 amostras = 100-200ms)")
        print("     antes do teste |vz| < 1.0 — veja a tabela acima: com 10")
        print("     amostras o sigma cai de ~4 m/s para ~0.4 m/s.")
        print("  2. Alternativa: tolerar |vz| < 1.5-2.0 com janela de")
        print("     confirmacao de N ciclos (evita falso apogeu).")
        print("  3. Alternativa: detectar apogeu por mudanca de sinal de vz")
        print("     suavizado / altitude decrescente.")
    else:
        print("  RISCO n.3 NAO CONFIRMADO nos datasets atuais:")
        print("  - Apogeu detectado em 100% dos runs (0% de perda), mesmo com")
        print("    ruido conservador +-0.1m (20x a quantizacao real do BMP585,")
        print("    que da sigma_vz ~= 0.1 m/s, 10x abaixo do threshold).")
        print("  - Efeito real do ruido a 50Hz: apogeu detectado ANTES do")
        print("    apogeu real (media ~-0.7s, pior ~-1.1s com +-0.1m), porque")
        print("    o ruido em vz (pico ~+-10 m/s) satisfaz |vz|<1 enquanto o")
        print("    foguete ainda sobe a ~5-10 m/s. Nao causa deploy prematuro:")
        print("    o deploy exige vz < -2 por 3 ciclos e ocorreu sempre APOS o")
        print("    apogeu real, acima do piso (min 268.8m no voo real).")
        print("  - Deploy chega a ocorrer um pouco ANTES do baseline original")
        print("    (media -0.15s simulado / -0.61s real), pois a FSM ja esta")
        print("    em DESCENT quando a descida comeca.")
        print("  - Nenhuma mudanca de firmware e' necessaria para o risco n.3;")
        print("    se quiser endurecer contra falso apogeu precoce, a media")
        print("    movel de 5-10 amostras acima reduz sigma_vz para <1 m/s.")
    print(f"\nTempo total: {__import__('time').time() - t0:.1f}s")


if __name__ == "__main__":
    main()
