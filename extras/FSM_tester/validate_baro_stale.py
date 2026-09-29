"""
validate_baro_stale.py

Valida a contingencia de barometro congelado (IMU-only) contra os datasets
reais, ANTES de tocar no firmware (padrao validar-antes-de-editar).

RISCO N 1 (analise de voo real): BMP585Sensor::update() preserva o ULTIMO
VALOR BOM quando a leitura falha - altitude/vz CONGELAM (nunca NaN). A FSM e o
backstop de queda livre dependem AMBOS de vz/altitude do barometro: se ele
congela em voo (I2C glitch, solda fria, EMI), nenhuma validacao de NaN pega,
a FSM nunca ve vz < -2 e o backstop nunca ve vz < -5 -> o paraquedas NAO abre,
com a telemetria parecendo normal. O IMU (LSM6DS3) continua vivo.

Contingencia (porta fiel do C++ proposto, FlightControlTask::checkBaroStaleContingency):
  por ciclo, com filtro IIR PROPRIO (alpha = FILTER_ALPHA, seed na 1a leitura):
    armada quando TODAS:
      baro stale:  idade da ultima leitura valida >= BARO_STALE_AGE_MS (2.0s)
      liftoff latched: acc filtrado > LIFTOFF_ACCEL_THRESHOLD (15 m/s2) ao
                       menos uma vez desde o boot (guarda obrigatoria: nunca
                       abrir no solo)
      maxAltitude (ultimo valor bom do baro) > BARO_STALE_MIN_HEIGHT (50m):
                   o foguete realmente voou acima da guarda de solo (sem
                   barometro vivo nao ha altura para checar no momento do
                   disparo - esta guarda substitui o piso)
    dispara quando armada E acc filtrado < BARO_STALE_ACC_THRESHOLD (3 m/s2)
    por BARO_STALE_SUSTAIN_S (2.5s) - janela MAIOR que o backstop (1s) porque
    sem barometro nao ha guarda vz < 0 nem altura viva.

Cenarios:
  A) Voo normal (barometro vivo): contingencia NUNCA dispara; FSM/backstop
     seguem como antes (validadores existentes nao quebram).
  B) Barometro congela NO APOGEU (RocketPy + voo real): SEM a contingencia
     NENHUM deploy (gap: FSM presa com vz congelado ~0; backstop preso com
     vz congelado > -5). COM a contingencia, deploy na descida acima do piso.
  C) Barometro congela NO INICIO DA DESCIDA (vz congelado em (-2, 0), antes
     do cruzamento do deploy): SEM -> gap; COM -> deploy na descida acima do
     piso. So roda onde o dataset tem amostra com vz em (-2, 0) apos o apogeu
     (o voo real cruza -2 em 1 amostra, entao C fica restrito a simulacao).
  D) Bancada (13_30_11) com barometro MORTO desde o boot: a contingencia
     NUNCA dispara - mesmo com bump de acc > 15 (latch de liftoff arma) e
     janela acc < 3 de 8.6s, a guarda de maxAltitude (~0 m) bloqueia o
     disparo. Nenhum deploy em solo.

Usage: python3 validate_baro_stale.py
"""

import csv
import math

from validate_freefall_backstop import (
    FSM,
    FreefallBackstop,
    LIFTOFF_ACCEL_THRESHOLD,
    PARACHUTE_MIN_ALTITUDE,
    resample_50hz,
    total_accel,
)

BASE = "/home/vinicius/Documentos/projects/flight-computer/extras/FSM_tester/"

# ── Thresholds (espelho do config.h proposto) ──────────────────────────────
BARO_STALE_AGE_MS       = 2000    # ms sem leitura valida => barometro congelado
BARO_STALE_ACC_THRESHOLD = 3.0    # m/s2  near zero-g (mesmo do backstop)
BARO_STALE_MIN_HEIGHT    = 50.0   # m     guarda de solo via maxAltitude (ultimo valor bom)
BARO_STALE_SUSTAIN_S     = 2.5    # s     janela sustentada (2.5s @ 50Hz = 125 ciclos)
FILTER_ALPHA             = 0.2


def load(path):
    """Le o CSV, normaliza 'millis' para SEGUNDOS e nivela altp ao solo.

    Nota de unidades: dados_simulados.csv e dados_filtrados.csv tem millis em
    SEGUNDOS (0.0, 0.1, ...); 13_30_11-Dados.csv tem em MILISSEGUNDOS
    (7060, 7260, ...). Normaliza pelo max(t) > 1000.
    """
    rows = []
    with open(path) as f:
        r = csv.DictReader(f)
        # Autodetect: simulacoes RocketPy usam time/z; telemetria usa millis/altp
        tcol = "time" if "time" in (r.fieldnames or []) else "millis"
        hcol = "z" if "z" in (r.fieldnames or []) else "altp"
        for row in r:
            try:
                t = float(row[tcol])
                altp = float(row[hcol])
                ax = float(row["ax"])
                ay = float(row["ay"])
                az = float(row["az"])
            except (ValueError, KeyError):
                continue
            rows.append((t, altp, ax, ay, az))
    if not rows:
        return []
    if max(t for t, *_ in rows) > 1000.0:
        rows = [(t / 1000.0, altp, ax, ay, az) for (t, altp, ax, ay, az) in rows]
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


def compute_vz_list(rows):
    """vz por diferenciacao numerica com dt REAL (como BMP585Sensor::update)."""
    vz_list = []
    prev_t = prev_alt = None
    for (t, altp, ax, ay, az) in rows:
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        vz = ((altp - prev_alt) / dt) if (dt > 0) else 0.0
        vz_list.append(max(min(vz, 200.0), -200.0))
        prev_t, prev_alt = t, altp
    return vz_list


class BaroStaleContingency:
    """Porta fiel do checkBaroStaleContingency() proposto no FlightControlTask.

    Filtro IIR proprio (nao usa estado da FSM nem do backstop), latch de
    liftoff por acc filtrado > 15 m/s2, guarda de solo por maxAltitude, e
    janela sustentada de acc < 3 m/s2 enquanto o barometro estiver stale.
    """

    def __init__(self):
        self.first = True
        self.fax = self.fay = self.faz = 0.0
        self.liftoff_latched = False
        self.sustained = 0.0        # s de condicao continua (armada e acc<3)
        self.fired_at = None        # (t, h) no momento do disparo

    def update(self, t, baro_age_ms, max_alt, ax, ay, az, dt):
        if self.first:
            self.fax, self.fay, self.faz = ax, ay, az
            self.first = False
        else:
            self.fax = FILTER_ALPHA * ax + (1.0 - FILTER_ALPHA) * self.fax
            self.fay = FILTER_ALPHA * ay + (1.0 - FILTER_ALPHA) * self.fay
            self.faz = FILTER_ALPHA * az + (1.0 - FILTER_ALPHA) * self.faz
        acc = total_accel(self.fax, self.fay, self.faz)

        if acc > LIFTOFF_ACCEL_THRESHOLD:
            self.liftoff_latched = True

        armed = (self.liftoff_latched and
                 max_alt > BARO_STALE_MIN_HEIGHT and
                 baro_age_ms >= BARO_STALE_AGE_MS)
        if not armed:
            self.sustained = 0.0
        elif acc < BARO_STALE_ACC_THRESHOLD:
            self.sustained += dt
            if self.fired_at is None and self.sustained >= BARO_STALE_SUSTAIN_S:
                self.fired_at = (t, max_alt)
        else:
            self.sustained = 0.0
        return acc


def run_alive(path, name):
    """Cenario A: barometro vivo - contingencia nunca dispara, FSM deploya."""
    rows = load(path)
    vz_list = compute_vz_list(rows)
    fsm = FSM()
    backstop = FreefallBackstop()
    cont = BaroStaleContingency()
    apogee_t = apogee_h = None
    fsm_deploy_t = None
    prev_t = prev_alt = None
    peak = None
    for i, (t, altp, ax, ay, az) in enumerate(rows):
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        peak = altp if peak is None else max(peak, altp)
        deployed = fsm.update(altp, ax, ay, az, vz_list[i])
        backstop.update(t, altp, vz_list[i], ax, ay, az, dt)
        cont.update(t, 0.0, peak, ax, ay, az, dt)   # baro sempre fresco
        if fsm.apogee and apogee_t is None:
            apogee_t, apogee_h = t, altp
        if deployed and fsm_deploy_t is None:
            fsm_deploy_t = t
        prev_t, prev_alt = t, altp
    ok = (cont.fired_at is None and fsm_deploy_t is not None and
          apogee_t is not None and fsm_deploy_t > apogee_t)
    print(f"  [A] Voo normal ({name}): FSM deploy@{fsm_deploy_t}s apogeu@{apogee_t}s "
          f"contingencia={'DISPAROU' if cont.fired_at else 'nao disparou'}  "
          f"{'PASS' if ok else 'FAIL'}")
    return ok


def find_freeze_index(rows, vz_list, apogee_t, mode):
    """Indice k onde o barometro congela (sample k = ultima leitura valida;
    o congelamento vale do sample k+1 em diante)."""
    if mode == "apogeu":
        for i, (t, *_rest) in enumerate(rows):
            if t >= apogee_t - 1e-9:
                return i
        return None
    # mode == "descida": primeira amostra apos o apogeu com vz em (-2, 0)
    for i, (t, *_rest) in enumerate(rows):
        if t > apogee_t + 0.01 and -2.0 < vz_list[i] < 0.0:
            return i
    return None


def run_frozen(path, name, mode, with_contingency):
    """Cenarios B/C: barometro congela em voo. Retorna dict de resultados ou
    None se o dataset nao tem o ponto de congelamento para o modo."""
    rows = load(path)
    vz_list = compute_vz_list(rows)

    # 1) apogeu da FSM com dados reais
    fsm = FSM()
    apogee_t = apogee_h = None
    prev_t = prev_alt = None
    for i, (t, altp, ax, ay, az) in enumerate(rows):
        fsm.update(altp, ax, ay, az, vz_list[i])
        if fsm.apogee and apogee_t is None:
            apogee_t, apogee_h = t, altp
        prev_t, prev_alt = t, altp
    if apogee_t is None:
        return None

    # 2) ponto de congelamento
    k = find_freeze_index(rows, vz_list, apogee_t, mode)
    if k is None:
        return None
    frozen_alt = rows[k][1]
    frozen_vz = vz_list[k]
    peak = max(r[1] for r in rows[:k + 1])

    # 3) passada com barometro congelado a partir de k+1
    fsm2 = FSM()
    backstop = FreefallBackstop()
    cont = BaroStaleContingency() if with_contingency else None
    deploy_t = deploy_h = None
    prev_t = prev_alt = None
    for i, (t, altp, ax, ay, az) in enumerate(rows):
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        if i <= k:
            alt, vz = altp, vz_list[i]
            baro_age_ms = 0.0
        else:
            alt, vz = frozen_alt, frozen_vz
            baro_age_ms = (t - rows[k][0]) * 1000.0
        deployed = fsm2.update(alt, ax, ay, az, vz)
        backstop.update(t, alt, vz, ax, ay, az, dt)
        if cont is not None:
            cont.update(t, baro_age_ms, peak, ax, ay, az, dt)
        if deployed and deploy_t is None:
            deploy_t, deploy_h = t, alt
        prev_t, prev_alt = t, altp

    return {
        "apogee_t": apogee_t,
        "apogee_h": apogee_h,
        "frozen_t": rows[k][0],
        "frozen_vz": frozen_vz,
        "frozen_alt": frozen_alt,
        "deploy_t": deploy_t,
        "deploy_h": deploy_h,
        "cont_fired": cont.fired_at if cont else None,
    }


def check_frozen_case(path, name, mode, label):
    """Roda SEM (gap) e COM (contingencia) e imprime PASS/FAIL."""
    sem = run_frozen(path, name, mode, with_contingency=False)
    if sem is None:
        print(f"  [{label}] {name}: sem ponto de congelamento valido (n/a)")
        return True
    com = run_frozen(path, name, mode, with_contingency=True)

    gap_ok = sem["deploy_t"] is None and sem["cont_fired"] is None
    print(f"  [{label}] {name} (baro congela em t={sem['frozen_t']}s "
          f"vz={sem['frozen_vz']:.2f} h={sem['frozen_alt']:.1f}m, "
          f"apogeu t={sem['apogee_t']}s):")
    print(f"      SEM contingencia: deploy={'SIM' if sem['deploy_t'] else 'NAO'}  "
          f"{'GAP OK' if gap_ok else 'SEM GAP (nao ha risco)'}")

    if com["cont_fired"] is None:
        print(f"      COM contingencia: NAO disparou  {'FAIL' if gap_ok else 'n/a'}")
        return not gap_ok
    (ct, ch) = com["cont_fired"]
    fired_ok = (ct > com["apogee_t"] + 1e-9 and ch > PARACHUTE_MIN_ALTITUDE)
    print(f"      COM contingencia: deploy IMU-only em t={ct:.2f}s h={ch:.1f}m "
          f"({'apos' if ct > com['apogee_t'] else 'ANTES do'} apogeu, "
          f"{'acima' if ch > PARACHUTE_MIN_ALTITUDE else 'ABAIXO do'} piso)  "
          f"{'PASS' if (gap_ok and fired_ok) else 'FAIL'}")
    return gap_ok and fired_ok


def run_bench(path, name):
    """Cenario D: bancada com barometro morto desde o boot - nunca dispara."""
    rows = load(path)
    vz_list = compute_vz_list(rows)
    fsm = FSM()
    backstop = FreefallBackstop()
    cont = BaroStaleContingency()
    deploy_t = None
    prev_t = prev_alt = None
    frozen_alt = rows[0][1]
    frozen_vz = 0.0
    for i, (t, altp, ax, ay, az) in enumerate(rows):
        dt = (t - prev_t) if (prev_t is not None and t - prev_t > 0) else 0.0
        baro_age_ms = (t - rows[0][0]) * 1000.0   # morto desde o boot
        deployed = fsm.update(frozen_alt, ax, ay, az, frozen_vz)
        backstop.update(t, frozen_alt, frozen_vz, ax, ay, az, dt)
        cont.update(t, baro_age_ms, frozen_alt, ax, ay, az, dt)
        if deployed and deploy_t is None:
            deploy_t = t
        prev_t, prev_alt = t, altp
    fired = cont.fired_at is not None
    ok = (not fired) and (deploy_t is None)
    print(f"  [D] Bancada ({name}), baro morto: contingencia "
          f"{'DISPAROU' if fired else 'nao disparou'}, deploy "
          f"{'SIM' if deploy_t else 'NAO'}  {'PASS' if ok else 'FAIL'}")
    return ok


if __name__ == "__main__":
    results = []
    results.append(run_alive(BASE + "dados_simulados.csv", "RocketPy"))
    results.append(run_alive(BASE + "dados_filtrados.csv", "voo real"))
    results.append(run_alive(BASE + "flight_results_thonyan.csv", "sim Thonyan"))
    results.append(run_alive(BASE + "flight_results_dedalo.csv", "sim Dedalo"))
    print()
    results.append(check_frozen_case(BASE + "dados_simulados.csv",
                                     "RocketPy", "apogeu", "B"))
    results.append(check_frozen_case(BASE + "dados_filtrados.csv",
                                     "voo real", "apogeu", "B"))
    print()
    results.append(check_frozen_case(BASE + "dados_simulados.csv",
                                     "RocketPy", "descida", "C"))
    results.append(check_frozen_case(BASE + "dados_filtrados.csv",
                                     "voo real", "descida", "C"))
    print()
    results.append(run_bench(BASE + "13_30_11-Dados.csv", "13_30_11"))
    print(f"\n>>> {'TODOS OS CENARIOS BARO-STALE PASS' if all(results) else 'ALGUM CENARIO FALHOU'}")
