"""
validate_parachute_option_a.py

Valida a logica da Opcao A para deploy de paraquedas no apogeu:
  - Deploy acontece logo apos o apogeu confirmado (vz negativo estavel),
    NAO na subida e NAO perto do solo.
  - Porta fiel da logica C++ (FlightStateMachine.cpp) com os thresholds da
    Opcao A em config.h.

Modelo de voo (LSM6DS3 reporta ACCELERACAO TOTAL, inclui gravidade ~+9.81 m/s2
no eixo "cima" quando em repouso no solo):
  - t=0..3s   : solo, az ~ +9.81 (g), vz=0
  - t=3..6s   : motor, az ~ +25 (empuxo), sobe
  - t=6..6.3s : coasting, az cai para ~ -9.81 (transicao)
  - t=6.3..12s: queda livre, az ~ -9.81, vz negativo crescente
O apogeu ocorre quando vz ~ 0 (topo), com az ja negativo.

Integracao: dv/dt = az_sensor - g   (g=+9.81, pois az ja inclui g no solo)
            dh/dt = vz

Caso de SEGURANCA (solo): vibracao onde a accel TOTAL some <= 15 m/s2
(ou seja, perturbacao de no maximo ~5 m/s2 sobre a gravidade) -> NAO deve
gerar liftoff falso nem deploy.
"""

import math

# ── Thresholds (espelho de config.h Opcao A) ────────────────────────────────
LIFTOFF_ACCEL_THRESHOLD = 15.0
APOGEE_MAX_VZ = 1.0
FILTER_ALPHA = 0.2
PARACHUTE_MIN_ALTITUDE = 50.0   # piso de solo (nunca abrir abaixo)
PARACHUTE_CONFIRM_VZ = -2.0     # vz negativo estavel (m/s)
PARACHUTE_CONFIRM_CYCLES = 3    # N ciclos de confirmacao apos apogeu

G = 9.81

# ── Helpers (port fiel do C++) ──────────────────────────────────────────────
def smooth(value, prev, alpha=FILTER_ALPHA):
    if prev is None:
        return value
    return alpha * value + (1.0 - alpha) * prev

def total_accel(ax, ay, az):
    return math.sqrt(ax * ax + ay * ay + az * az)

IDLE, ASCENT, DESCENT, LANDED = 0, 1, 2, 3
NAME = {IDLE: "IDLE", ASCENT: "ASCENT", DESCENT: "DESCENT", LANDED: "LANDED"}

class FSM:
    def __init__(self):
        self.state = IDLE
        self.liftoff = self.burnout = self.apogee = self.freefall = False
        self.parachute = False
        self.fax = self.fay = self.faz = None
        self.first = True
        self.para_confirm = 0
        self.entered = 0.0
        self.t = 0.0
        self.h = 0.0
        self.vz = 0.0

    def detect_liftoff(self):
        return total_accel(self.fax, self.fay, self.faz) > LIFTOFF_ACCEL_THRESHOLD

    def detect_apogee(self, vz):
        return (abs(vz) < APOGEE_MAX_VZ)

    # Opcao A: deploy apos apogeu + vz negativo estavel + acima do piso de solo
    def detect_parachute(self, height, vz):
        return (height > PARACHUTE_MIN_ALTITUDE and vz < PARACHUTE_CONFIRM_VZ)

    def transition(self, nxt):
        print(f"  [{self.t:6.2f}s] {NAME[self.state]} -> {NAME[nxt]} "
              f"(h={self.h:.1f} vz={self.vz:.2f})")
        self.state = nxt
        self.entered = self.t
        if nxt == DESCENT:
            self.para_confirm = 0  # reseta contador no apogeu

    def update(self, height, ax, ay, az, vz):
        self.h = height
        self.vz = vz
        if self.first:
            self.fax, self.fay, self.faz = ax, ay, az
            self.first = False
        else:
            self.fax = smooth(ax, self.fax)
            self.fay = smooth(ay, self.fay)
            self.faz = smooth(az, self.faz)
        acc = total_accel(self.fax, self.fay, self.faz)

        if self.state == IDLE:
            if not self.liftoff and self.detect_liftoff():
                self.liftoff = True
                self.transition(ASCENT)
        elif self.state == ASCENT:
            if not self.burnout and height >= 5.0 and vz > 0.5 and \
               (self.faz < -8.0 or acc < 2.0):
                self.burnout = True
                print(f"  [{self.t:6.2f}s] BURNOUT h={height:.1f} vz={vz:.2f}")
            if not self.apogee and self.detect_apogee(vz):
                self.apogee = True
                self.transition(DESCENT)
        elif self.state == DESCENT:
            if not self.freefall and height >= 5.0 and vz < -5.0 and acc < 11.5:
                self.freefall = True
                print(f"  [{self.t:6.2f}s] FREEFALL h={height:.1f} vz={vz:.2f}")
            if not self.parachute:
                if self.detect_parachute(height, vz):
                    self.para_confirm += 1
                    if self.para_confirm >= PARACHUTE_CONFIRM_CYCLES:
                        self.parachute = True
                        print(f"  [{self.t:6.2f}s] *** PARACHUTE DEPLOYED "
                              f"h={height:.1f} vz={vz:.2f} "
                              f"(apogeu em ~{self.entered:.2f}s) ***")
                else:
                    self.para_confirm = 0
            if self.parachute and height < 2.0 and abs(vz) < 0.5:
                self.transition(LANDED)
        return self.parachute


def sensor_accel(t):
    """Retorna (az) do sensor LSM6DS3 (accel total) no instante t."""
    if t < 3.0:
        return G                      # solo em repouso
    if 3.0 <= t < 6.0:
        return 25.0                   # motor
    if 6.0 <= t < 6.3:
        # transicao suave do empuxo para queda livre
        return G - (G - (-G)) * ((t - 6.0) / 0.3)
    return -G                         # queda livre


def synthetic_flight():
    dt = 0.02
    rows = []
    alt = 0.0
    vz = 0.0
    for i in range(int(12.0 / dt)):
        t = i * dt
        az = sensor_accel(t)
        ax = ay = 0.02
        if t < 3.0:
            vz = 0.0
            alt = 0.0
        else:
            vz += (az - G) * dt       # dv/dt = az - g
            vz = max(min(vz, 200.0), -200.0)
            alt += vz * dt
            alt = max(alt, 0.0)
        rows.append((t, alt, ax, ay, az, vz))
    return rows


def soil_vibration_test():
    """Seguranca: vibracao no solo onde accel TOTAL some no maximo ~15 m/s2.
    Perturbacao de +5 sobre a gravidade (9.81+5=14.81 < 15) -> sem liftoff."""
    dt = 0.02
    rows = []
    for i in range(int(5.0 / dt)):
        t = i * dt
        # perturbacao periodica de +5 m/s2 sobre a gravidade (total < 15)
        az = G + (5.0 if i % 37 == 0 else 0.0)
        ax = ay = 0.0
        rows.append((t, 0.0, ax, ay, az, 0.0))
    return rows


def sensor_accel_high(t):
    """Voo de ALTO apogeu (~1000m): motor forte e queima longa para atingir
    ~1000m de apogeu (vz de saida ~140 m/s)."""
    if t < 3.0:
        return G
    if 3.0 <= t < 10.0:
        return 30.0                  # empuxo: accel liquida ~20 m/s2
    if 10.0 <= t < 10.3:
        return G - (G - (-G)) * ((t - 10.0) / 0.3)  # transicao p/ queda livre
    return -G


def high_apogee_flight():
    dt = 0.02
    rows = []
    alt = 0.0
    vz = 0.0
    for i in range(int(20.0 / dt)):
        t = i * dt
        az = sensor_accel_high(t)
        ax = ay = 0.02
        if t < 3.0:
            vz = 0.0
            alt = 0.0
        else:
            vz += (az - G) * dt
            vz = max(min(vz, 200.0), -200.0)
            alt += vz * dt
            alt = max(alt, 0.0)
        rows.append((t, alt, ax, ay, az, vz))
    return rows


def run(label, rows):
    print(f"\n=== {label} ===")
    fsm = FSM()
    deploy_t = None
    apogee_t = None
    deploy_h = None
    for (t, alt, ax, ay, az, vz) in rows:
        fsm.t = t
        deployed = fsm.update(alt, ax, ay, az, vz)
        if fsm.apogee and apogee_t is None:
            apogee_t = t
        if deployed and deploy_t is None:
            deploy_t = t
            deploy_h = alt
    print(f"  -> apogeu={apogee_t}s deploy={deploy_t}s @h="
          f"{(deploy_h if deploy_h is not None else 0.0):.1f}m "
          f"liftoff={fsm.liftoff} apogeu_flag={fsm.apogee} "
          f"parachute={fsm.parachute} state={NAME[fsm.state]}")
    return deploy_t, apogee_t, deploy_h


if __name__ == "__main__":
    d1, a1, h1 = run("VOO SINTETICO (apogeu baixo)", synthetic_flight())
    d2, a2, h2 = run("VOO ALTO (~1000m apogeu)", high_apogee_flight())
    d3, a3, h3 = run("SEGURANCA: vibracao no solo (<15 m/s2 total)", soil_vibration_test())

    print("\n=== RESULTADO ===")
    # Voo baixo: deploy APOS apogeu.
    ok_low = (d1 is not None) and (a1 is not None) and (d1 > a1)
    # Voo alto: deploy APOS apogeu E com altitude ALTA (perto do topo, nao a 50m).
    ok_high = (d2 is not None) and (a2 is not None) and (d2 > a2) and (h2 > 200.0)
    # Solo: nenhum liftoff, nenhum deploy.
    ok_soil = (d3 is None)
    print(f"Voo baixo: deploy apos apogeu (d1>a1) ? {ok_low} "
          f"(apogeu={a1}s deploy={d1}s @h={h1:.1f}m)")
    print(f"Voo alto : deploy apos apogeu e ALTO (h>200m) ? {ok_high} "
          f"(apogeu={a2}s deploy={d2}s @h={h2:.1f}m)")
    print(f"Solo     : nenhum deploy ? {ok_soil} (deploy={d3})")
    print("\nPASS" if (ok_low and ok_high and ok_soil) else "\nFAIL")
