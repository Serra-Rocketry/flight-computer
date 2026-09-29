"""
validate_arming.py — Risco Nº2: armamento na rampa

Problema (risco real de voo):
  1. A vibracao de bancada (13_30_11-Dados.csv, spikes de ~22-122 m/s2) dispara
     liftoff FALSO: o filtro IIR (alpha=0.2) passa de 15 m/s2 (LIFTOFF_ACCEL_
     THRESHOLD) e a FSM entra em ASCENT no laboratorio.
  2. O snapshot NVS da FSM (namespace 'flight', chave 'fsm') so e' limpo quando
     a FSM chega a LANDED. Se o ESP32 reiniciar (ou desligar) DEPOIS do liftoff
     falso e ANTES do LANDED, o boot seguinte restaura ASCENT.
  3. Na rampa, com ASCENT restaurado: |vz| ~ 0 no pad -> detectApogee dispara
     na hora -> DESCENT -> detectLanded (|vz|<0.5 e h<2) -> LANDED. A FSM fica
     em LANDED e o paraquedas NUNCA abre no voo real.
  4. base_pressure e' capturada UMA vez no boot (BMP585Sensor::begin). Drift de
     pressao (~1 hPa/dia, 1 hPa ~ 8.4 m) desloca a altitude relativa — um voo
     baixo pode nem cruzar PARACHUTE_MIN_ALTITUDE.

Fix validado aqui (port fiel do C++ a implementar):
  - Comando 'ARM' via Serial (antes do liftoff): limpa o snapshot NVS
    (FlightStateMachine::reset -> IDLE, flags zeradas), re-captura
    base_pressure da pressao atual (setBasePressure(getPressure())) e zera
    maxAltitude. Idempotente; negado se o voo ja comecou de verdade
    (maxAltitude > ARM_MAX_ARM_ALTITUDE ou parachute ja aberto).
  - Complemento: auto re-zero no boot — com a FSM em IDLE, se a altitude
    relativa ficar < -10 m por 3.0 s continuos (drift de pressao), re-captura
    base_pressure automaticamente.

Cenarios (criterio PASS: deploy no APOGEU, acima do piso de solo 50m):
  (a) Voo normal com armamento no boot          -> deploy no apogeu.
  (b) NVS sujo (ASCENT de voo anterior) + ARM   -> volta a IDLE, voo limpo.
      (controle SEM ARM: LANDED no pad, sem deploy — prova o gap)
  (c) base_pressure desatualizada (+2 hPa ~ +17 m) + ARM -> altitude
      re-nivelada, deploy no apogeu correto (vs baseline).
      (controle SEM ARM: deploy deslocado ~ -17 m — prova o gap)
  (d) Bancada vibrante + ARM no campo           -> sem liftoff falso herdado,
      voo real limpo com deploy no apogeu.
      (prova do risco: a bancada dispara liftoff falso; controle SEM ARM:
      restaura ASCENT -> LANDED no pad -> sem deploy)
  (e) Auto re-zero (complemento): drift + SEM ARM -> re-nivela sozinho apos
      3.0 s, deploy no apogeu correto.

Rodar:
  cd extras/FSM_tester && python3 validate_arming.py
"""
import csv
import math
import sys

sys.path.insert(0, ".")
from validate_parachute_realflight import FSM, total_accel, smooth, \
    IDLE, ASCENT, DESCENT, LANDED, PARACHUTE_CONFIRM_CYCLES, \
    PARACHUTE_MIN_ALTITUDE, PARACHUTE_CONFIRM_VZ

# ── Thresholds (espelho de config.h — armamento na rampa) ──────────────────
ARM_MAX_ARM_ALTITUDE = 5.0       # m  negar ARM acima disto (voo ja comecou)
ARM_AUTO_REZERO_ALTITUDE = -10.0  # m  drift de pressao assumido abaixo disto
ARM_AUTO_REZERO_CYCLES = 150      # 3.0 s @ 50 Hz (FLIGHT_CONTROL_PERIOD_MS=20ms)

NAME = {IDLE: "IDLE", ASCENT: "ASCENT", DESCENT: "DESCENT", LANDED: "LANDED"}


# ── Carga de dados ──────────────────────────────────────────────────────────

def load(path):
    """Retorna [(t, altp_abs, ax, ay, az)] — sem nivelar (o simulador nivela)."""
    rows = []
    with open(path) as f:
        for row in csv.DictReader(f):
            try:
                t = float(row["millis"])
                altp = float(row["altp"])
                ax = float(row["ax"])
                ay = float(row["ay"])
                az = float(row["az"])
            except (ValueError, KeyError):
                continue
            rows.append((t, altp, ax, ay, az))
    return rows


def with_pad_hold(rows, hold_s=8.0, dt=0.02):
    """Precede o voo com uma seccao de rampa (foguete parado, az ~ g).

    O BMP585/IMU reais tem o foguete parado na rampa por alguns segundos
    antes do liftoff; os datasets (RocketPy/voo real) comecam ja em queima.
    altp constante -> vz=0 no pad (deterministico para o cenario (b) SEM ARM).

    NOTA: so e' viavel para datasets com queima longa (dados_simulados).
    Em dados_filtrados a fase de empuxo com accel > 15 m/s2 dura ~1.2 s —
    com o filtro IIR ja seedado pelo hold (9.81 m/s2), o valor filtrado nao
    volta a cruzar 15 e o liftoff nunca e' detectado (artefato do dataset).
    """
    alt0 = rows[0][1]
    t0 = rows[0][0]
    n = int(hold_s / dt)
    pad = [(i * dt, alt0, 0.0, 0.0, 9.81) for i in range(n)]
    shift = hold_s * 1000.0 - t0   # voo comeca apos o hold
    return pad + [(t + shift, altp, ax, ay, az) for (t, altp, ax, ay, az) in rows]


def pad_only(rows, hold_s=5.0, dt=0.02):
    """Sequeencia de rampa estatica (sem voo) — teste do mecanismo de
    auto re-zero: drift presente, FSM em IDLE, sem liftoff."""
    alt0 = rows[0][1]
    return [(i * dt, alt0, 0.0, 0.0, 9.81) for i in range(int(hold_s / dt))]


def _f(value, spec):
    """Formata valor possivelmente None (prints robustos)."""
    return "None" if value is None else format(value, spec)


# ── FSM port + armamento (espelho do C++ a implementar) ────────────────────

class ArmingFSM(FSM):
    """FSM da Opcao A + armamento na rampa (Risco Nº2).

    - arm(): reset p/ IDLE + flags zeradas + filtro re-seedado
      (port de FlightStateMachine::arm() -> reset(); o re-nivelamento do
      barometro e' feito pelo FlightSim, como setBasePressure no C++).
    - snapshot()/restore(): snapshot NVS (port de persistToNVS/
      restoreFromNVS — estado + flags + contador de confirmacao).
    - detect_landed: transicao DESCENT -> LANDED (port de detectLanded;
      o port base de validate_parachute_realflight nao tem essa transicao).
    """

    def __init__(self):
        super().__init__()
        self.landed_det = False

    def arm(self):
        self.state = IDLE
        self.liftoff = self.burnout = self.apogee = self.freefall = False
        self.parachute = False
        self.para_confirm = 0
        self.fax = self.fay = self.faz = None
        self.first = True
        self.landed_det = False

    def snapshot(self):
        return {"state": self.state, "liftoff": self.liftoff,
                "burnout": self.burnout, "apogee": self.apogee,
                "freefall": self.freefall, "parachute": self.parachute,
                "confirm": self.para_confirm}

    def restore(self, snap):
        self.state = snap["state"]
        self.liftoff = snap["liftoff"]
        self.burnout = snap["burnout"]
        self.apogee = snap["apogee"]
        self.freefall = snap["freefall"]
        self.parachute = snap["parachute"]
        self.para_confirm = snap["confirm"]
        self.fax = self.fay = self.faz = None
        self.first = True
        self.landed_det = False

    def update(self, height, ax, ay, az, vz):
        deployed = super().update(height, ax, ay, az, vz)
        # LANDED (FlightStateMachine.cpp: detectLanded roda em DESCENT)
        if self.state == DESCENT and not self.landed_det:
            if abs(vz) < 0.5 and height < 2.0:
                self.landed_det = True
                self.transition(LANDED)
        return deployed


class FlightSim:
    """Barometro (referencia base) + FSM + NVS + armamento — port fiel.

    boot:    base = altp[0] + drift  (BMP585Sensor::begin captura a pressao;
             'drift' modela base_pressure desatualizada, +2 hPa ~ +17 m)
    ARM:     fsm.arm() + base = altp atual (setBasePressure(getPressure()))
             + maxAltitude zerado -> altitude re-nivelada a ~0
    auto:    FSM em IDLE com h < -10 m por ARM_AUTO_REZERO_CYCLES ciclos
             continuos -> re-captura base (auto re-zero do boot)
    reboot:  restore(snap NVS) no inicio (comportamento watchdog)
    """

    def __init__(self, drift=0.0, restore_snap=None, arm_at_boot=False,
                 auto_rezero=False, verbose=False):
        self.fsm = ArmingFSM()
        self.drift = drift
        self.base = None
        self.prev_t = self.prev_alt = None
        self.deploy_t = self.deploy_h = None
        self.apogee_t = self.apogee_h = None
        self.arm_pending = arm_at_boot
        self.auto_rezero = auto_rezero
        self.rezero_count = 0
        self.rezero_t = None
        self.nvs = None
        self.verbose = verbose
        if restore_snap is not None:
            self.fsm.restore(restore_snap)
            self.nvs = dict(restore_snap)

    def request_arm(self):
        """Comando 'ARM' via Serial a qualquer momento antes do liftoff."""
        self.arm_pending = True

    def step(self, t, altp, ax, ay, az):
        if self.base is None:
            self.base = altp + self.drift     # boot: base_pressure (+ drift)

        if self.arm_pending:
            self.fsm.arm()                    # limpa NVS + flags + IDLE
            self.base = altp                  # re-captura pressao atual
            self.drift = 0.0
            self.arm_pending = False
            self.nvs = None
            self.prev_t, self.prev_alt = t, 0.0   # sem spike de vz
            if self.verbose:
                print(f"    [sim] ARM @ t={t:.2f}s -> IDLE, base re-capturada")

        h = altp - self.base
        if self.prev_t is not None and (t - self.prev_t) > 0:
            dt = t - self.prev_t
            vz = (h - self.prev_alt) / dt
            vz = max(min(vz, 200.0), -200.0)
        else:
            vz = 0.0

        # Auto re-zero (C++: case IDLE — h < -10 m por N ciclos continuos)
        if self.auto_rezero and self.fsm.state == IDLE and \
           h < ARM_AUTO_REZERO_ALTITUDE:
            self.rezero_count += 1
            if self.rezero_count >= ARM_AUTO_REZERO_CYCLES:
                self.rezero_count = 0
                self.base = altp
                self.prev_t, self.prev_alt = t, 0.0
                h = 0.0
                self.rezero_t = t
                if self.verbose:
                    print(f"    [sim] AUTO RE-ZERO @ t={t:.2f}s (drift)")
        else:
            self.rezero_count = 0

        self.fsm.t = t
        deployed = self.fsm.update(h, ax, ay, az, vz)

        # Snapshot NVS (persistToNVS: transicoes + deploy)
        if self.nvs is None or self.nvs["state"] != self.fsm.state or \
           (self.fsm.parachute and not self.nvs["parachute"]):
            self.nvs = self.fsm.snapshot()

        if self.fsm.apogee and self.apogee_t is None:
            self.apogee_t, self.apogee_h = t, h
        if deployed and self.deploy_t is None:
            self.deploy_t, self.deploy_h = t, h

        self.prev_t, self.prev_alt = t, h
        return deployed

    def run(self, rows):
        for (t, altp, ax, ay, az) in rows:
            self.step(t, altp, ax, ay, az)


# ── Cenarios ────────────────────────────────────────────────────────────────

def _deploy_ok(sim):
    """Deploy no apogeu e acima do piso de solo."""
    return (sim.fsm.parachute and sim.deploy_t is not None and
            sim.apogee_t is not None and sim.deploy_t > sim.apogee_t and
            sim.deploy_h is not None and sim.deploy_h > PARACHUTE_MIN_ALTITUDE)


def scenario_a(flight, label):
    """(a) Voo normal com armamento no boot — deploy no apogeu."""
    sim = FlightSim(arm_at_boot=True, verbose=True)
    sim.run(flight)
    ok = _deploy_ok(sim)
    print(f"(a) VOO NORMAL + ARMAMENTO no boot [{label}]")
    print(f"    apogeu t={sim.apogee_t:.2f}s h={sim.apogee_h:.1f}m | "
          f"deploy t={sim.deploy_t:.2f}s h={sim.deploy_h:.1f}m | "
          f"estado final {NAME[sim.fsm.state]}")
    print(f"    >> {'PASS' if ok else 'FAIL'} (deploy no apogeu, acima de "
          f"{PARACHUTE_MIN_ALTITUDE:.0f}m)")
    return ok


def scenario_b(flight, label):
    """(b) NVS sujo (ASCENT de voo anterior) + ARMAMENTO -> voo limpo."""
    dirty = {"state": ASCENT, "liftoff": True, "burnout": False,
             "apogee": False, "freefall": False, "parachute": False,
             "confirm": 0}
    # Controle SEM ARMAMENTO (comportamento atual): restaura ASCENT
    sim_no = FlightSim(restore_snap=dict(dirty))
    sim_no.run(flight)
    # COM ARMAMENTO: ARM limpa o snapshot -> IDLE -> voo limpo
    sim_yes = FlightSim(restore_snap=dict(dirty), arm_at_boot=True)
    sim_yes.run(flight)
    gap = not sim_no.fsm.parachute          # sem ARM: nunca abre (prova o gap)
    ok = _deploy_ok(sim_yes) and gap
    print(f"(b) NVS SUJO (ASCENT) + ARMAMENTO [{label}]")
    print(f"    SEM ARM: restaura ASCENT -> {NAME[sim_no.fsm.state]} na rampa, "
          f"parachute={sim_no.fsm.parachute} (gap: sem deploy no voo real)")
    print(f"    COM ARM: IDLE -> apogeu t={sim_yes.apogee_t:.2f}s "
          f"h={sim_yes.apogee_h:.1f}m | deploy t={sim_yes.deploy_t:.2f}s "
          f"h={sim_yes.deploy_h:.1f}m | estado final {NAME[sim_yes.fsm.state]}")
    print(f"    >> {'PASS' if ok else 'FAIL'} (ARM limpa ASCENT herdado)")
    return ok


def scenario_c(flight, label, drift=17.0):
    """(c) base_pressure desatualizada (+2 hPa ~ +17 m) + ARMAMENTO."""
    sim_base = FlightSim()                    # referencia: sem drift, sem ARM
    sim_base.run(flight)
    sim_no = FlightSim(drift=drift)           # SEM ARM: drift persiste
    sim_no.run(flight)
    sim_yes = FlightSim(drift=drift, arm_at_boot=True)  # COM ARM: re-nivela
    sim_yes.run(flight)
    d_base = sim_base.deploy_h
    ok = (_deploy_ok(sim_yes) and d_base is not None and
          abs(sim_yes.deploy_h - d_base) < 3.0 and
          sim_no.deploy_h is not None and
          abs(sim_no.deploy_h - d_base) > 5.0)
    print(f"(c) BASE_PRESSURE DESATUALIZADA (+{drift:.0f}m) + ARMAMENTO "
          f"[{label}]")
    print(f"    baseline deploy h={d_base:.1f}m | SEM ARM deploy "
          f"h={sim_no.deploy_h:.1f}m (deslocado {sim_no.deploy_h - d_base:+.1f}m) "
          f"| COM ARM deploy h={sim_yes.deploy_h:.1f}m "
          f"({sim_yes.deploy_h - d_base:+.1f}m)")
    print(f"    >> {'PASS' if ok else 'FAIL'} (ARM re-nivela a altitude, "
          f"deploy no apogeu correto)")
    return ok


def scenario_d(bench, flight, label):
    """(d) Bancada vibrante + ARMAMENTO no campo — sem liftoff falso herdado."""
    # 1) Prova o risco: a vibracao da bancada dispara liftoff FALSO
    sim_bench = FlightSim()
    false_liftoff_t = None
    for (t, altp, ax, ay, az) in bench:
        sim_bench.step(t, altp, ax, ay, az)
        if sim_bench.fsm.liftoff and false_liftoff_t is None:
            false_liftoff_t = t
            break
    # 2) Reboot no meio da sessao de bancada: NVS ficou em ASCENT
    dirty = sim_bench.fsm.snapshot()
    # Controle SEM ARMAMENTO: restaura ASCENT -> LANDED na rampa -> sem deploy
    sim_no = FlightSim(restore_snap=dict(dirty))
    sim_no.run(flight)
    # COM ARMAMENTO: ARM no campo -> IDLE -> voo limpo -> deploy no apogeu
    sim_yes = FlightSim(restore_snap=dict(dirty), arm_at_boot=True)
    sim_yes.run(flight)
    gap = false_liftoff_t is not None and not sim_no.fsm.parachute
    ok = gap and _deploy_ok(sim_yes)
    print(f"(d) BANCADA VIBRANTE + ARMAMENTO no campo [voo: {label}]")
    print(f"    vibracao da bancada: liftoff FALSO em t={false_liftoff_t}ms "
          f"(risco provado)")
    print(f"    SEM ARM: restaura {NAME[dirty['state']]} -> "
          f"{NAME[sim_no.fsm.state]} na rampa, parachute={sim_no.fsm.parachute}")
    print(f"    COM ARM: apogeu t={sim_yes.apogee_t:.2f}s "
          f"h={sim_yes.apogee_h:.1f}m | deploy t={sim_yes.deploy_t:.2f}s "
          f"h={sim_yes.deploy_h:.1f}m | estado final {NAME[sim_yes.fsm.state]}")
    print(f"    >> {'PASS' if ok else 'FAIL'} (ARM limpa o liftoff falso "
          f"da bancada)")
    return ok


def scenario_e(flight, label, drift=17.0, pad_only=False):
    """(e) Auto re-zero no boot (complemento): drift + SEM ARM.

    pad_only=True: rampa estatica (sem voo) — valida apenas o mecanismo
    (re-zero dispara, altitude volta a ~0, FSM segue IDLE). Usado em
    datasets cuja fase de empuxo curta nao re-dispara o liftoff apos o
    hold (dados_filtrados — artefato do dataset, ver with_pad_hold).
    """
    sim_base = FlightSim()
    sim_base.run(flight)
    sim_yes = FlightSim(drift=drift, auto_rezero=True)  # com auto re-zero
    sim_yes.run(flight)
    if pad_only:
        h_end = sim_yes.fsm.h
        ok = (sim_yes.rezero_t is not None and not sim_yes.fsm.liftoff and
              sim_yes.fsm.state == IDLE and h_end > -1.0)
        print(f"(e) AUTO RE-ZERO no boot (drift +{drift:.0f}m, SEM ARM) "
              f"[{label}] — mecanismo (rampa estatica)")
        print(f"    re-zero automatico em t={_f(sim_yes.rezero_t, '.2f')}s | "
              f"h final={h_end:.2f}m | estado {NAME[sim_yes.fsm.state]} | "
              f"liftoff={sim_yes.fsm.liftoff}")
        print(f"    >> {'PASS' if ok else 'FAIL'} (re-nivela sozinho apos "
              f"{ARM_AUTO_REZERO_CYCLES / 50.0:.1f}s de drift, sem liftoff)")
        return ok
    sim_no = FlightSim(drift=drift)                 # sem auto re-zero
    sim_no.run(flight)
    d_base = sim_base.deploy_h
    ok = (sim_yes.rezero_t is not None and _deploy_ok(sim_yes) and
          d_base is not None and abs(sim_yes.deploy_h - d_base) < 3.0 and
          sim_no.deploy_h is not None and abs(sim_no.deploy_h - d_base) > 5.0)
    print(f"(e) AUTO RE-ZERO no boot (drift +{drift:.0f}m, SEM ARM) [{label}]")
    print(f"    re-zero automatico em t={_f(sim_yes.rezero_t, '.2f')}s | "
          f"baseline deploy h={_f(d_base, '.1f')}m | SEM auto re-zero deploy "
          f"h={_f(sim_no.deploy_h, '.1f')}m "
          f"({_f(sim_no.deploy_h - d_base if sim_no.deploy_h is not None and d_base is not None else None, '+.1f')}m) "
          f"| COM auto re-zero deploy h={_f(sim_yes.deploy_h, '.1f')}m "
          f"({_f(sim_yes.deploy_h - d_base if sim_yes.deploy_h is not None and d_base is not None else None, '+.1f')}m)")
    print(f"    >> {'PASS' if ok else 'FAIL'} (re-nivela sozinho apos "
          f"{ARM_AUTO_REZERO_CYCLES / 50.0:.1f}s de drift)")
    return ok


def run_dataset(flight, bench, label, pad_only_e):
    print(f"\n{'=' * 74}\nDATASET: {label}\n{'=' * 74}")
    r = {}
    r["a"] = scenario_a(flight, label)
    r["b"] = scenario_b(flight, label)
    r["c"] = scenario_c(flight, label)
    r["d"] = scenario_d(bench, flight, label)
    e_seq = pad_only(flight) if pad_only_e else flight
    r["e"] = scenario_e(e_seq, label, pad_only=pad_only_e)
    return r


if __name__ == "__main__":
    BASE = "/home/vinicius/Documentos/projects/flight-computer/extras/FSM_tester/"
    bench = load(BASE + "13_30_11-Dados.csv")
    # Simulado: hold de 8s na rampa (queima longa -> liftoff re-detectado).
    # Real (dados_filtrados): sem hold — a fase de empuxo com accel>15 dura
    # ~1.2s e o filtro IIR seedado pelo hold nao re-cruza 15 (artefato do
    # dataset, ver with_pad_hold); (e) vira teste de mecanismo na rampa.
    datasets = [
        (with_pad_hold(load(BASE + "dados_simulados.csv")),
         "VOO SIMULADO (RocketPy, apogeu ~951m)", False),
        (load(BASE + "dados_filtrados.csv"),
         "VOO REAL (apogeu ~272m)", True),
    ]
    all_ok = True
    for flight, label, pad_only_e in datasets:
        res = run_dataset(flight, bench, label, pad_only_e)
        all_ok &= all(res.values())
    print(f"\n{'=' * 74}\n>>> "
          f"{'TODOS OS CENARIOS PASS' if all_ok else 'ALGUM CENARIO FALHOU'}"
          f"\n{'=' * 74}")
    sys.exit(0 if all_ok else 1)
