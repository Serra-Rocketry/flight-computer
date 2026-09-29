"""
validate_watchdog_reboot.py

Valida o FIX de persistencia de estado da FSM (NVS) contra reboots do watchdog
no meio do voo, usando dados_simulados.csv (RocketPy, apogeu ~951m nivelado) e
dados_filtrados.csv (voo real, ~272m).

Cenario: o watchdog reinicia o ESP32 em t_reboot (fases: queima, coasting,
pre-apogeu, pos-apogeu, pos-deploy). Compara:

  SEM FIX (comportamento atual):
    - FSM volta a IDLE, flags zeradas, filtro re-seedado
    - BMP585 re-nivela a altitude ao ponto do reboot (base_pressure capturada
      de novo) -> altitude relativa ao ponto de reboot

  COM FIX (proposto):
    - Estado/flags/contador restaurados do snapshot NVS
    - base_pressure restaurada -> altitude continua absoluta ao local de lancamento
    - vz correto desde a 1a amostra pos-reboot (prev_alt = altitude no reboot)

Criterio PASS: paraquedas abre no apogeu (> piso 50m) em todos os cenarios
COM FIX; SEM FIX deve FALHAR em reboot durante coasting/descida (prova o gap).
"""
import csv
import math
import sys

sys.path.insert(0, ".")
from validate_parachute_realflight import FSM, total_accel, smooth, \
    IDLE, ASCENT, DESCENT, LANDED, PARACHUTE_CONFIRM_CYCLES, \
    PARACHUTE_MIN_ALTITUDE, PARACHUTE_CONFIRM_VZ, resample_50hz


def load(path, leveled=True):
    """Retorna [(t, altp_abs, ax, ay, az)]; altp nivelado se leveled=True"""
    rows = []
    with open(path) as f:
        r = csv.DictReader(f)
        # Autodetect: simulacoes RocketPy usam time/z; telemetria usa millis/altp
        tcol = "time" if "time" in (r.fieldnames or []) else "millis"
        hcol = "z" if "z" in (r.fieldnames or []) else "altp"
        for row in r:
            try:
                t = float(row[tcol]); altp = float(row[hcol])
                ax = float(row["ax"]); ay = float(row["ay"]); az = float(row["az"])
            except (ValueError, KeyError):
                continue
            rows.append((t, altp, ax, ay, az))
    if leveled and rows:
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


class FSMWithPersistence(FSM):
    """FSM + snapshot NVS (port do fix proposto no C++)."""

    def snapshot(self):
        return {
            "state": self.state,
            "liftoff": self.liftoff, "burnout": self.burnout,
            "apogee": self.apogee, "freefall": self.freefall,
            "parachute": self.parachute, "confirm": self.para_confirm,
        }

    def restore(self, snap):
        self.state = snap["state"]
        self.liftoff = snap["liftoff"]
        self.burnout = snap["burnout"]
        self.apogee = snap["apogee"]
        self.freefall = snap["freefall"]
        self.parachute = snap["parachute"]
        self.para_confirm = snap["confirm"]
        # filtro re-seedado (equivalente a _firstReading=true no boot)
        self.fax = self.fay = self.faz = None
        self.first = True


def run_flight(rows, reboot_idx=None, with_fix=False, base_offset=0.0):
    """
    Roda a FSM sobre rows. Se reboot_idx: injeta reboot nesse indice.
    base_offset: deslocamento a aplicar na altitude (re-nivelamento).
    Retorna (parachute_deployed, deploy_t, deploy_h, state_final)
    """
    fsm = FSMWithPersistence()
    snap = None
    prev_t = prev_alt = None
    deploy_t = deploy_h = None
    base = base_offset

    for i, (t, altp, ax, ay, az) in enumerate(rows):
        # ── injecao de reboot ──
        if reboot_idx is not None and i == reboot_idx:
            if with_fix:
                # restaura estado; altitude continua absoluta (base_pressure NVS)
                fsm.restore(snap if snap else fsm.snapshot())
                base = 0.0  # sem re-nivelamento
            else:
                # SEM FIX: FSM nova (IDLE) + baro re-nivelado ao ponto do reboot
                fsm = FSMWithPersistence()
                base = -altp  # altitude relativa ao ponto do reboot
            # prev_alt = altitude no instante do reboot (vz correto dali em diante)
            prev_t, prev_alt = t, altp + base
            continue

        h = altp + base
        if prev_t is not None and (t - prev_t) > 0:
            dt = t - prev_t
            vz = (h - prev_alt) / dt
            vz = max(min(vz, 200.0), -200.0)
        else:
            vz = 0.0
        fsm.t = t
        deployed = fsm.update(h, ax, ay, az, vz)
        # snapshot NVS: salvo em transicoes e no deploy (como o C++ fara)
        if snap is None or snap["state"] != fsm.state or \
           (fsm.parachute and not snap["parachute"]):
            snap = fsm.snapshot()
        if deployed and deploy_t is None:
            deploy_t, deploy_h = t, h
        prev_t, prev_alt = t, h

    return fsm.parachute, deploy_t, deploy_h, fsm.state


def pick_reboot_idx(rows, target_t):
    """Indice da amostra mais proxima de target_t."""
    return min(range(len(rows)), key=lambda i: abs(rows[i][0] - target_t))


def eval_dataset(path, reboot_times, label):
    rows = load(path)
    apogee = max(r[1] for r in rows)
    print(f"\n{'='*72}\n{label}: {path}  (apogeu nivelado ~{apogee:.0f}m, "
          f"{len(rows)} amostras, dt={rows[1][0]-rows[0][0]:.2f}s)\n{'='*72}")

    # baseline sem reboot
    ch, ct, chh, st = run_flight(rows)
    print(f"\n[baseline]  parachute={ch}  deploy t={ct}s "
          f"h={chh if chh is not None else float('nan'):.1f}m  "
          f"(apogeu ~{apogee:.0f}m)  -> {'PASS' if ch and chh is not None and chh > PARACHUTE_MIN_ALTITUDE else 'FAIL'}")

    all_ok = True
    for tt in reboot_times:
        idx = pick_reboot_idx(rows, tt)
        t_actual = rows[idx][0]
        ch_old, _, _, st_old = run_flight(rows, reboot_idx=idx, with_fix=False)
        ch_new, dt_new, dh_new, st_new = run_flight(rows, reboot_idx=idx, with_fix=True)
        ok = ch_new and dh_new is not None and dh_new > PARACHUTE_MIN_ALTITUDE
        all_ok &= ok
        print(f"\n[reboot t={t_actual:5.1f}s]  SEM FIX: parachute={ch_old} "
              f"(estado final {st_old})")
        print(f"                    COM FIX: parachute={ch_new}  deploy t={dt_new}s "
              f"h={dh_new if dh_new is not None else float('nan'):.1f}m "
              f"-> {'PASS' if ok else 'FAIL'}")
        if ch_old:
            print("  (nota: SEM FIX abriu — reboot durante a queima, janela de sorte)")
    print(f"\n>>> {'TODOS OS CENARIOS COM FIX PASS' if all_ok else 'ALGUM CENARIO FALHOU'}")
    return all_ok


if __name__ == "__main__":
    BASE = "/home/vinicius/Documentos/projects/flight-computer/extras/FSM_tester/"
    ok1 = eval_dataset(
        BASE + "dados_simulados.csv",
        reboot_times=[1.5, 6.0, 13.0, 15.0, 16.5],
        label="VOO SIMULADO (RocketPy)")
    ok2 = eval_dataset(
        BASE + "dados_filtrados.csv",
        reboot_times=[1.0, 4.0, 6.5, 8.0],
        label="VOO REAL")
    ok3 = eval_dataset(
        BASE + "flight_results_thonyan.csv",
        reboot_times=[1.5, 8.0, 11.6, 12.5, 15.0],
        label="SIM Thonyan (~682m)")
    ok4 = eval_dataset(
        BASE + "flight_results_dedalo.csv",
        reboot_times=[2.0, 12.0, 17.5, 18.5, 22.0],
        label="SIM Dedalo (~1544m)")
    sys.exit(0 if (ok1 and ok2 and ok3 and ok4) else 1)
