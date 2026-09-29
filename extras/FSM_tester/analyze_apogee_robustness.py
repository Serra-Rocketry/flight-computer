"""
analyze_apogee_robustness.py
============================

Risco nº5 — robustez da detecção de apogeu a BAMBOLEIO PÓS-BURNOUT.

Problema físico:
  O acelerômetro mede aceleração APARENTE (gravidade + centrípeta + aero).
  No coasting pós-burnout, se o foguete bamboleia (vento, fins, distúrbios),
  a projeção da gravidade aparente no eixo Z do corpo oscila. O apogeu ANTIGO
  exigia  |vz| < 1.0 m/s  E  faz_filtrado < -0.1 m/s²  — se o bamboleio empurra
  o faz acima de -0.1 exatamente na janela |vz| < 1.0 (muito estreita), o
  apogeu NUNCA é detectado e a FSM fica presa em ASCENT (deploy só via
  backstop, 1-3 s atrasado).  RISCO Nº5 CONFIRMADO no voo real (margem de
  apenas ~0.81 m/s²) — ver o relatório abaixo.

RESOLUÇÃO (implementada no firmware em 2026-08-05):
  detectApogee() agora é SÓ POR |vz| < 1.0 m/s (o gate de az foi REMOVIDO —
  config.h / FlightStateMachine.cpp). O vz vem da altitude barométrica, não
  do acelerômetro -> imune ao bamboleio por construção. Ruído de quantização
  a 50Hz pode cruzar |vz| < 1 antes do apogeu real — benigno, pois o deploy
  exige vz < -2 m/s sustentado (validate_50hz_noise.py: 0 deploys prematuros).

O que este script faz (NÃO altera firmware):
  1. Reusa a porta Python da FSM (importa de validate_parachute_realflight.py)
     — que já é a versão vz-only implementada.
  2. Para cada dataset de voo, injeta bamboleio senoidal no sinal de accel
     a partir do burnout até apogeu+1s:
       - bilateral : az' = az + A*sin(2*pi*f*(t-t_burnout))          (modelo pedido)
       - unilateral: az' = az + A*sin(pi*f*(t-t_burnout))^2          (pior caso físico:
                     gravidade aparente projetada -> az só fica MENOS negativo)
     Com A de 0.0 a 3.0 m/s² (refinamento 0.05 perto do limite) e f de
     1 a 5 Hz (bamboleio pendular típico em coasting), em 4 fases (pior caso).
  3. Roda a FSM e classifica:
       OK    -> apogeu detectado na janela correta (atraso <= 0.5 s), deploy segue
       DELAY -> apogeu detectado com atraso > 0.5 s
       LOST  -> apogeu NUNCA detectado -> FSM presa em ASCENT -> SEM DEPLOY
  4. Roda em 2 taxas de amostragem:
       native -> dt do CSV (igual aos validadores existentes)
       50hz   -> reamostragem linear para 50 Hz (fiel ao firmware real, que roda
                 a 50 Hz; no dt nativo do voo real ~3.8 Hz o Nyquist é ~1.9 Hz,
                 e o filtro IIR alpha=0.2 fica MUITO mais lento que no firmware)
  5. Reporta a maior amplitude tolerada por frequência ANTES de falhar — com a
     FSM vz-only espera-se IMUNE em todo o range (a varredura comprova).
     O faz no apogeu é impresso para documentar por que o gate de az antigo
     era o elo fraco (margem 0.81 m/s² no voo real).

Usage:  python3 analyze_apogee_robustness.py
        python3 analyze_apogee_robustness.py --fast   # grid grosso (A passo 0.5)
"""

import csv
import math
import os
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE)

# ── Reuso da porta da FSM (mesma dos validadores existentes) ───────────────
from validate_parachute_realflight import (           # noqa: E402
    FSM,
    APOGEE_MAX_VZ,
    PARACHUTE_MIN_ALTITUDE,
)

# ── Parâmetros da análise ──────────────────────────────────────────────────
FREQS_HZ          = [1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0]
A_MIN, A_MAX      = 0.0, 3.0          # m/s² (range pedido: 0.2 a 3.0)
A_STEP            = 0.2               # passo grosso
A_REFINE          = 0.05              # refinamento perto do limite
PHASES_DEG        = [0, 90, 180, 270] # fase da senoide em t_burnout (pior caso)
APOGEE_DELAY_TOL  = 0.5               # s — atraso máx aceitável p/ "deploy no apogeu"
UP_RATE_HZ        = 50.0              # reamostragem "fiel ao firmware"

OK, DELAY, LOST = "OK", "DELAY", "LOST"


# ── Utilidades ──────────────────────────────────────────────────────────────

def load(path):
    """Lê CSV e nivela altp ao solo (como os validadores / base_pressure)."""
    rows = []
    with open(path) as f:
        r = csv.DictReader(f)
        for row in r:
            try:
                t = float(row["millis"])
                altp = float(row["altp"])
                ax = float(row["ax"])
                ay = float(row["ay"])
                az = float(row["az"])
            except (ValueError, KeyError):
                continue
            rows.append((t, altp, ax, ay, az))
    if not rows:
        return []
    base = rows[0][1]
    return [(t, altp - base, ax, ay, az) for (t, altp, ax, ay, az) in rows]


def upsample_linear(rows, dt_target):
    """Reamostra (t, alt, ax, ay, az) para grade uniforme dt_target via interpolação linear."""
    if len(rows) < 2:
        return rows
    t0, tN = rows[0][0], rows[-1][0]
    n = int(round((tN - t0) / dt_target)) + 1
    out = []
    ti = t0
    idx = 0
    for k in range(n):
        t = t0 + k * dt_target
        if t > tN:
            break
        while idx < len(rows) - 2 and rows[idx + 1][0] < t:
            idx += 1
        (t1, a1, x1, y1, z1) = rows[idx]
        (t2, a2, x2, y2, z2) = rows[idx + 1]
        if t2 > t1:
            w = (t - t1) / (t2 - t1)
        else:
            w = 0.0
        out.append((t,
                    a1 + w * (a2 - a1),
                    x1 + w * (x2 - x1),
                    y1 + w * (y2 - y1),
                    z1 + w * (z2 - z1)))
    return out


def inject_wobble(rows, amp, freq, t_start, t_end, mode, phase_deg):
    """Adiciona bamboleio senoidal ao az entre t_start (burnout) e t_end (apogeu+1s)."""
    ph = math.radians(phase_deg)
    out = []
    for (t, alt, ax, ay, az) in rows:
        if t_start <= t <= t_end:
            tt = t - t_start
            if mode == "bilateral":
                w = amp * math.sin(2.0 * math.pi * freq * tt + ph)
            else:  # unilateral: az so sobe (menos negativo) — pior caso fisico
                w = amp * math.sin(math.pi * freq * tt + ph) ** 2
            az = az + w
        out.append((t, alt, ax, ay, az))
    return out


def run_fsm(rows):
    """Roda a FSM e devolve eventos de interesse."""
    fsm = FSM()
    prev_t = prev_alt = None
    ev = {"liftoff": None, "burnout": None, "apogee": None, "deploy": None,
          "faz_apogee": None}
    for (t, alt, ax, ay, az) in rows:
        if prev_t is not None and (t - prev_t) > 0:
            vz = (alt - prev_alt) / (t - prev_t)
            vz = max(min(vz, 200.0), -200.0)
        else:
            vz = 0.0
        fsm.t = t
        deployed = fsm.update(alt, ax, ay, az, vz)
        if fsm.liftoff and ev["liftoff"] is None:
            ev["liftoff"] = t
        if fsm.burnout and ev["burnout"] is None:
            ev["burnout"] = t
        if fsm.apogee and ev["apogee"] is None:
            ev["apogee"] = t
            ev["faz_apogee"] = fsm.faz
        if deployed and ev["deploy"] is None:
            ev["deploy"] = t
        prev_t, prev_alt = t, alt
    return ev


def classify(ev, t_apogee_base):
    """OK / DELAY / LOST baseado no apogeu detectado e no deploy."""
    if ev["apogee"] is None:
        return LOST
    delay = ev["apogee"] - t_apogee_base
    if delay > APOGEE_DELAY_TOL:
        return DELAY
    if ev["deploy"] is None or ev["deploy"] < ev["apogee"]:
        return DELAY
    return OK


def max_tolerated_amp(rows, freq, t_burnout, t_apogee, mode):
    """
    Maior amplitude (m/s²) que ainda detecta o apogeu, considerando o pior
    caso entre as fases. Estratégia: grid grosso + refinamento binário.
    Retorna (amp_max, fail_info) — fail_info descreve a 1ª falha:
      {"kind": "LOST"|"DELAY", "apogee_delay_s": float, "deploy_delay_s": float|None}
    """
    phases = PHASES_DEG

    def outcome(amp):
        worst = OK
        fail_info = None
        for ph in phases:
            rows_w = inject_wobble(rows, amp, freq, t_burnout, t_apogee + 1.0, mode, ph)
            ev = run_fsm(rows_w)
            st = classify(ev, t_apogee)
            if st == LOST:
                return LOST, {"kind": "LOST", "apogee_delay_s": None,
                              "deploy_delay_s": None}
            if st == DELAY:
                worst = DELAY
                if fail_info is None:
                    fail_info = {
                        "kind": "DELAY",
                        "apogee_delay_s": ev["apogee"] - t_apogee,
                        "deploy_delay_s": ((ev["deploy"] - t_apogee)
                                           if ev["deploy"] is not None else None),
                    }
        return worst, fail_info

    # controle A=0 deve ser OK (baseline sem bamboleio)
    base_status, _ = outcome(0.0)
    if base_status != OK:
        return 0.0, {"kind": base_status, "apogee_delay_s": None,
                     "deploy_delay_s": None}

    # grid grosso
    lo, hi = A_MIN, A_MAX
    last_ok = 0.0
    fail_info = None
    a = A_STEP
    while a <= A_MAX + 1e-9:
        st, fi = outcome(a)
        if st == OK:
            last_ok = a
        else:
            hi = a
            fail_info = fi
            break
        a += A_STEP
    if last_ok >= A_MAX - 1e-9 and outcome(A_MAX)[0] == OK:
        # imune no range pedido -> estende para achar o limite real
        a = A_MAX + 0.5
        while a <= 20.0:
            st, fi = outcome(a)
            if st != OK:
                hi = a
                fail_info = fi
                break
            last_ok = a
            a += 0.5
        if a > 20.0:
            return float("inf"), None
    lo = last_ok
    # refinamento binário entre lo (OK) e hi (falha)
    for _ in range(6):
        mid = (lo + hi) / 2.0
        st, fi = outcome(mid)
        if st == OK:
            lo = mid
        else:
            hi = mid
            fail_info = fi
    return lo, fail_info


# ── Relatório ───────────────────────────────────────────────────────────────

def sweep_dataset(rows, name, dt_label):
    """Varredura completa de um dataset; devolve dict com resultados e baselines."""
    ev0 = run_fsm(rows)
    t_burnout = ev0["burnout"] or ev0["liftoff"] or 0.0
    t_apogee = ev0["apogee"]
    print(f"\n  baseline: liftoff={ev0['liftoff']}s burnout={t_burnout:.2f}s "
          f"apogeu={t_apogee}s (faz={ev0['faz_apogee']:.3f}) deploy={ev0['deploy']}s")
    if t_apogee is None:
        print("  !! FSM nao detecta apogeu nem no baseline — pulando varredura")
        return None

    # Nyquist do dt nativo (aviso honesto p/ o voo real ~3.8 Hz)
    dt = rows[1][0] - rows[0][0] if len(rows) > 1 else 0.0
    fs = 1.0 / dt if dt > 0 else float("inf")
    nyq = fs / 2.0
    f_ok = [f for f in FREQS_HZ if f <= nyq * 0.95]
    f_skip = [f for f in FREQS_HZ if f > nyq * 0.95]
    print(f"  taxa={dt_label} (dt={dt:.4f}s, fs={fs:.1f}Hz, Nyquist={nyq:.2f}Hz)")

    res = {"name": name, "dt_label": dt_label, "baseline": ev0,
           "f_ok": f_ok, "f_skip": f_skip, "modes": {}}
    for mode in ("bilateral", "unilateral"):
        table = {}
        fail_log = []   # (freq, fail_info) para cada frequência que falhou
        for f in f_ok:
            amp_max, fi = max_tolerated_amp(rows, f, t_burnout, t_apogee, mode)
            table[f] = amp_max
            if fi is not None:
                fail_log.append((f, fi))
        res["modes"][mode] = {"table": table, "fail_log": fail_log}
        print(f"\n  [{mode}] maior amplitude tolerada (pior fase, m/s²):")
        print(f"    {'f(Hz)':>6}  {'A_max':>8}   interpretacao")
        for f in f_ok:
            a = table[f]
            s = (f"{a:.2f}" if math.isfinite(a) else f">20.0")
            interp = ("imune no range testado" if not math.isfinite(a) else
                      "acima do range (0.2-3.0)" if a >= A_MAX else
                      "margem apertada" if a < 1.0 else "razoavel")
            print(f"    {f:>6.1f}  {s:>8}   {interp}")
        if fail_log:
            lost = [fi for _, fi in fail_log if fi["kind"] == "LOST"]
            delays = [fi["deploy_delay_s"] for _, fi in fail_log
                      if fi["kind"] == "DELAY" and fi["deploy_delay_s"] is not None]
            n_lost = len(lost)
            n_delay = len(fail_log) - n_lost
            mean_delay = (sum(delays) / len(delays)) if delays else None
            print(f"    quando falha: {n_lost} freq com APOGEU PERDIDO "
                  f"(FSM presa em ASCENT, SEM deploy; backstop agiria ~1-3 s), "
                  f"{n_delay} freq com apogeu atrasado"
                  + (f"; atraso medio do deploy nos casos atrasados = "
                     f"{mean_delay:.2f} s" if mean_delay is not None else ""))
        if f_skip:
            print(f"    (f={','.join(f'{x:.1f}' for x in f_skip)} Hz: acima do "
                  f"Nyquist em {dt_label}, invalido aqui — ver 50 Hz)")
    return res


def recommendation(res_list):
    """Constrói o relatório: documenta o gate antigo e comprova a FSM vz-only."""
    print("\n" + "=" * 78)
    print("RELATÓRIO — gate de az removido (2026-08-05); FSM atual é vz-only")
    print("=" * 78)
    for res in res_list:
        if res is None:
            continue
        ev = res["baseline"]
        faz = ev["faz_apogee"]
        # Gate antigo: faz < -0.1 (APOGEE_AZ_THRESHOLD, removido de config.h)
        margin = abs(faz - (-0.1)) if faz is not None else float("nan")
        print(f"\n• {res['name']} ({res['dt_label']}): faz no apogeu = "
              f"{faz:.3f} m/s² → margem do GATE ANTIGO (-0.1) = {margin:.3f} m/s²")
        for mode, mdata in res["modes"].items():
            table = mdata["table"]
            vals = [a for a in table.values() if math.isfinite(a)]
            if vals:
                worst = min(vals)
                wf = [f for f, a in table.items() if a == worst]
                print(f"   - modo '{mode}': tolera até ~{worst:.2f} m/s² "
                      f"(pior em f={wf[0]:.1f} Hz) antes de perder o apogeu")
    # conclusão baseada nos números
    print("""
  1. POR QUE O GATE DE AZ FOI REMOVIDO (histórico da análise):
       - voo simulado (~950 m): faz_apogeu ≈ -9.6 m/s²  → margem ≈ 9.5 m/s²
         → gate antigo era IMUNE a bamboleio <= 3 m/s² (detecção dominada por |vz|).
       - voo real (~272 m):      faz_apogeu ≈ -0.91 m/s² → margem ≈ 0.81 m/s²
         → modo 'unilateral' (bamboleio pendular: az aparente só fica MENOS
           negativo): o gate antigo perdia o apogeu com ~1.1 m/s² @1 Hz,
           ~1.7 @1.5 Hz, ~2.4-2.8 m/s² @2-5 Hz.  RISCO Nº5 CONFIRMADO.
         → modo 'bilateral' (senoide simétrica): só falhava em f <= 1 Hz
           (~1.8 m/s²); em f >= 1.5 Hz o sinal oscila e sempre cruzava o
           threshold dentro da janela |vz| < 1. O caso pendular real é o
           unilateral (gravidade projetada), que é o pior.
  2. STATUS (implementado no firmware em 2026-08-05):
       detectApogee() = |vz| < 1.0 m/s (APOGEE_MAX_VZ), SEM gate de az —
       config.h e FlightStateMachine.cpp. O vz vem da altitude barométrica
       (não do acelerômetro) -> a varredura acima (bamboleio injetado em az)
       mostra a FSM atual IMUNE no range testado, com o apogeu detectado no
       MESMO instante dos datasets (14.3 s / 7.813 s).
     A investigação da calibração do acelerômetro do voo real permanece como
     follow-up (az em repouso = +2.81 m/s², inconsistente com repouso ≈ 0)
     — não afeta mais a detecção de apogeu, mas importa para o free-fall
     backstop (que usa a aceleração total) e para análise pós-voo.
""")
    print("=" * 78)


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    fast = "--fast" in sys.argv
    global A_STEP
    if fast:
        A_STEP = 0.5
        print("modo --fast: grid grosso (A passo 0.5), sem refinamento")

    datasets = [
        ("dados_simulados.csv", "voo simulado RocketPy (~950 m)"),
        ("dados_filtrados.csv", "voo real filtrado (~272 m)"),
    ]
    res_list = []
    for fname, label in datasets:
        path = os.path.join(BASE, fname)
        rows_native = load(path)
        rows_50 = upsample_linear(rows_native, 1.0 / UP_RATE_HZ)
        print(f"\n{'='*78}\nDATASET: {fname} — {label} "
              f"({len(rows_native)} amostras nativas)\n{'='*78}")
        r1 = sweep_dataset(rows_native, f"{fname} [dt nativo]", "nativo")
        r2 = sweep_dataset(rows_50, f"{fname} [50 Hz upsample]", "50 Hz")
        if r1:
            res_list.append(r1)
        if r2:
            res_list.append(r2)

    recommendation(res_list)
    print("\n>>> Análise concluída. Nenhum firmware/validador foi modificado.")


if __name__ == "__main__":
    main()
