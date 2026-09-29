"""
validate_freefall_backstop.py

Valida o backstop de queda livre (FSM-independent safety net) contra os
datasets reais ANTES de tocar no firmware (padrao validar-antes-de-editar).

Porta fiel do C++ proposto no FlightControlTask (50Hz):
  condicao por ciclo:  totalAccel(filtrado IIR) < FREEFALL_BACKSTOP_ACC_THRESHOLD
                       AND vz < FREEFALL_BACKSTOP_VZ
                       AND height > FREEFALL_BACKSTOP_MIN_HEIGHT
  disparo:             condicao sustentada por >= 1.0s (50 ciclos @ 50Hz)

Cenarios:
  A) Voo normal (FSM ok): deploy continua no apogeu via FSM; o backstop NUNCA
     dispara antes do apogeu (na subida/coasting) — se dispara, e' junto/logo
     apos o apogeu, acima do piso.
  B) FSM presa em IDLE (gap que o NVS nao cobre): backstop deploya na descida,
     acima do piso, a tempo (apos o apogeu).
  C) Falso positivo de burnout: com FSM presa em IDLE, o backstop NAO dispara
     enquanto vz ainda e' positivo (foguete subindo).
  D) Bancada/vibracao em solo (13_30_11-Dados.csv): backstop nunca dispara.

Usage: python3 validate_freefall_backstop.py
"""

import csv
import math

BASE = "/home/vinicius/Documentos/projects/flight-computer/extras/FSM_tester/"

# ── Thresholds (espelho do config.h proposto) ──────────────────────────────
FREEFALL_BACKSTOP_ACC_THRESHOLD = 3.0    # m/s²  near zero-g
FREEFALL_BACKSTOP_VZ             = -5.0   # m/s   descending at least this fast
FREEFALL_BACKSTOP_MIN_HEIGHT     = 50.0   # m     ground guard
FREEFALL_BACKSTOP_DURATION_S     = 1.0    # s     sustained window (50 cycles @ 50Hz)
FILTER_ALPHA                     = 0.2

# FSM thresholds (mesmo do validate_parachute_realflight.py)
LIFTOFF_ACCEL_THRESHOLD = 15.0
APOGEE_MAX_VZ = 1.0
PARACHUTE_MIN_ALTITUDE = 50.0
PARACHUTE_CONFIRM_VZ = -2.0
PARACHUTE_CONFIRM_CYCLES = 3

IDLE, ASCENT, DESCENT, LANDED = 0, 1, 2, 3


def total_accel(ax, ay, az):
    return math.sqrt(ax * ax + ay * ay + az * az)


class FreefallBackstop:
    """Porta fiel do detector proposto no FlightControlTask."""

    def __init__(self):
        self.first = True
        self.fax = self.fay = self.faz = 0.0
        self.sustained = 0.0   # tempo (s) de condicao continua
        self.fired_at = None   # (t, h)

    def update(self, t, h, vz, ax, ay, az, dt):
        if self.first:
            self.fax, self.fay, self.faz = ax, ay, az
            self.first = False
        else:
            self.fax = FILTER_ALPHA * ax + (1.0 - FILTER_ALPHA) * self.fax
            self.fay = FILTER_ALPHA * ay + (1.0 - FILTER_ALPHA) * self.fay
            self.faz = FILTER_ALPHA * az + (1.0 - FILTER_ALPHA) * self.faz
        acc = total_accel(self.fax, self.fay, self.faz)

        if (acc < FREEFALL_BACKSTOP_ACC_THRESHOLD and
                vz < FREEFALL_BACKSTOP_VZ and
                h > FREEFALL_BACKSTOP_MIN_HEIGHT):
            self.sustained += dt
        else:
            self.sustained = 0.0

        if self.fired_at is None and self.sustained >= FREEFALL_BACKSTOP_DURATION_S:
            self.fired_at = (t, h)
        return acc


class FSM:
    """Porta da FSM atual (mesma do validate_parachute_realflight.py)."""

    def __init__(self):
        self.state = IDLE
        self.liftoff = self.burnout = self.apogee = self.freefall = False
        self.parachute = False
        self.fax = self.fay = self.faz = None
        self.first = True
        self.para_confirm = 0

    def detect_liftoff(self):
        return total_accel(self.fax, self.fay, self.faz) > LIFTOFF_ACCEL_THRESHOLD

    def detect_apogee(self, vz):
        return (abs(vz) < APOGEE_MAX_VZ)

    def detect_parachute(self, height, vz):
        return (height > PARACHUTE_MIN_ALTITUDE and vz < PARACHUTE_CONFIRM_VZ)

    def update(self, height, ax, ay, az, vz):
        if self.first:
            self.fax, self.fay, self.faz = ax, ay, az
            self.first = False
        else:
            self.fax = FILTER_ALPHA * ax + (1.0 - FILTER_ALPHA) * self.fax
            self.fay = FILTER_ALPHA * ay + (1.0 - FILTER_ALPHA) * self.fay
            self.faz = FILTER_ALPHA * az + (1.0 - FILTER_ALPHA) * self.faz

        if self.state == IDLE:
            if not self.liftoff and self.detect_liftoff():
                self.liftoff = True
                self.state = ASCENT
        elif self.state == ASCENT:
            if not self.apogee and self.detect_apogee(vz):
                self.apogee = True
                self.state = DESCENT
        elif self.state == DESCENT:
            if not self.parachute:
                if self.detect_parachute(height, vz):
                    self.para_confirm += 1
                    if self.para_confirm >= PARACHUTE_CONFIRM_CYCLES:
                        self.parachute = True
                else:
                    self.para_confirm = 0
        return self.parachute


def load(path, alt_col="altp"):
    rows = []
    with open(path) as f:
        r = csv.DictReader(f)
        # Autodetect: simulacoes RocketPy usam time/z; telemetria usa millis/altp
        tcol = "time" if "time" in (r.fieldnames or []) else "millis"
        if "z" in (r.fieldnames or []):
            alt_col = "z"
        for row in r:
            try:
                t = float(row[tcol])          # coluna e' SECONDS neste dataset
                altp = float(row[alt_col])
                ax = float(row["ax"]); ay = float(row["ay"]); az = float(row["az"])
            except (ValueError, KeyError):
                continue
            rows.append((t, altp, ax, ay, az))
    if rows:
        base = rows[0][1]
        rows = [(t, altp - base, ax, ay, az) for (t, altp, ax, ay, az) in rows]
    # Simulacoes RocketPy com passo adaptativo de ~2ms na subida: re-amostra a
    # 50Hz quando qualquer parte do voo tem resolucao mais fina que o firmware
    # (p10 dos dts < 10ms) — senao o vz por diferenca fica < 1 m/s apos o
    # liftoff e o apogeu dispara falso (o firmware so ve medias de 20ms).
    dts = sorted(b[0] - a[0] for a, b in zip(rows, rows[1:]) if b[0] - a[0] > 0)
    if dts and dts[len(dts) // 10] < 0.010:
        rows = resample_50hz(rows)
    return rows


def resample_50hz(rows):
    """Grade fixa dt=20ms (firmware roda a 50Hz) interpolando linearmente."""
    import numpy as np

    ts = np.array([r[0] for r in rows])
    h = np.array([r[1] for r in rows])
    ax = np.array([r[2] for r in rows])
    ay = np.array([r[3] for r in rows])
    az = np.array([r[4] for r in rows])
    n = int(round((ts[-1] - ts[0]) / 0.02)) + 1
    grid = ts[0] + np.arange(n) * 0.02
    grid = grid[grid <= ts[-1] + 1e-9]
    return [(t, float(hg), float(axg), float(ayg), float(azg))
            for t, hg, axg, ayg, azg in
            zip(grid, np.interp(grid, ts, h), np.interp(grid, ts, ax),
                np.interp(grid, ts, ay), np.interp(grid, ts, az))]


def run_flight(path, name):
    rows = load(path)
    print(f"\n=== {name}: {path.split('/')[-1]} ({len(rows)} amostras) ===")

    fsm = FSM()
    backstop = FreefallBackstop()
    apogee_t = apogee_h = None
    fsm_deploy_t = fsm_deploy_h = None
    prev_t = prev_alt = None

    # primeira passada: FSM normal (cenario A)
    for (t, altp, ax, ay, az) in rows:
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        vz = ((altp - prev_alt) / dt) if (dt > 0) else 0.0
        vz = max(min(vz, 200.0), -200.0)
        deployed = fsm.update(altp, ax, ay, az, vz)
        backstop.update(t, altp, vz, ax, ay, az, dt)
        if fsm.apogee and apogee_t is None:
            apogee_t, apogee_h = t, altp
        if deployed and fsm_deploy_t is None:
            fsm_deploy_t, fsm_deploy_h = t, altp
        prev_t, prev_alt = t, altp

    bt, bh = backstop.fired_at or (None, None)
    print(f"  FSM      : liftoff={fsm.liftoff} apogeu@t={apogee_t}s h={apogee_h} "
          f"deploy@t={fsm_deploy_t}s h={fsm_deploy_h}")
    print(f"  BACKSTOP : fired@t={bt}s h={bh}")

    ok_a = True
    reasons = []
    if bt is not None:
        if apogee_t is not None and bt < apogee_t:
            ok_a = False
            reasons.append("backstop disparou ANTES do apogeu (subida!)")
        if bh is not None and bh < PARACHUTE_MIN_ALTITUDE:
            ok_a = False
            reasons.append("backstop disparou abaixo do piso")
        # em voo normal, backstop nao deve disparar BEM antes do deploy da FSM
        if fsm_deploy_t is not None and bt < fsm_deploy_t - 2.0:
            ok_a = False
            reasons.append(f"backstop muito antes do deploy da FSM "
                           f"({fsm_deploy_t - bt:.2f}s)")
    print(f"  [A] FSM ok + backstop inofensivo: {'PASS' if ok_a else 'FAIL'}"
          + (f"  ({'; '.join(reasons)})" if reasons else ""))

    # segunda passada: FSM presa em IDLE (cenario B) — backstop sozinho
    backstop2 = FreefallBackstop()
    prev_t = prev_alt = None
    for (t, altp, ax, ay, az) in rows:
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        vz = ((altp - prev_alt) / dt) if (dt > 0) else 0.0
        vz = max(min(vz, 200.0), -200.0)
        backstop2.update(t, altp, vz, ax, ay, az, dt)
        prev_t, prev_alt = t, altp
    bt2, bh2 = backstop2.fired_at or (None, None)

    ok_b = bt2 is not None
    reasons_b = []
    if ok_b:
        if apogee_t is not None and bt2 < apogee_t:
            ok_b = False
            reasons_b.append("disparou antes do apogeu")
        if bh2 < PARACHUTE_MIN_ALTITUDE:
            ok_b = False
            reasons_b.append("abaixo do piso")
        # cenario C: NAO pode disparar enquanto vz > 0 (subida)
        # (checado implicitamente: disparo apos o apogeu implica vz < 0)
        if apogee_t is not None and bt2 - apogee_t > 30:
            reasons_b.append(f"(atencao: disparo {bt2 - apogee_t:.1f}s apos apogeu)")
        print(f"  [B] FSM presa em IDLE -> backstop deploya em t={bt2:.2f}s "
              f"h={bh2:.1f}m (apogeu t={apogee_t}s)  "
              f"{'PASS' if ok_b else 'FAIL'}"
              + (f"  ({'; '.join(reasons_b)})" if reasons_b else ""))
    else:
        print(f"  [B] FSM presa em IDLE -> backstop NAO disparou  "
              f"{'FAIL' if apogee_t else 'n/a'}")
        ok_b = False if apogee_t else True

    return ok_a, ok_b


def run_bench(path, name):
    """Cenario D: vibracao de bancada (sem voo) nunca dispara o backstop."""
    rows = load(path)
    backstop = FreefallBackstop()
    prev_t = prev_alt = None
    for (t, altp, ax, ay, az) in rows:
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        vz = ((altp - prev_alt) / dt) if (dt > 0) else 0.0
        vz = max(min(vz, 200.0), -200.0)
        backstop.update(t, altp, vz, ax, ay, az, dt)
        prev_t, prev_alt = t, altp
    fired = backstop.fired_at is not None
    print(f"  [D] Bancada ({name}): backstop {'DISPAROU' if fired else 'nao disparou'}  "
          f"{'FAIL' if fired else 'PASS'}")
    return not fired


if __name__ == "__main__":
    results = []
    results.append(run_flight(BASE + "dados_simulados.csv", "voo simulado RocketPy"))
    results.append(run_flight(BASE + "dados_filtrados.csv", "voo real"))
    results.append(run_flight(BASE + "flight_results_thonyan.csv", "sim Thonyan (~682m)"))
    results.append(run_flight(BASE + "flight_results_dedalo.csv", "sim Dedalo (~1544m)"))
    results.append((run_bench(BASE + "13_30_11-Dados.csv", "13_30_11"),))
    flat = [r for pair in results for r in (pair if isinstance(pair, tuple) else (pair,))]
    print(f"\n>>> {'TODOS OS CENARIOS COM FIX PASS' if all(flat) else 'ALGUM CENARIO FALHOU'}")
