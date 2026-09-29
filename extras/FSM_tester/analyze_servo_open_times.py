"""
analyze_servo_open_times.py

Analisa o log serial do teste de bancada do servo do paraquedas
(test/parachute_servo/parachute_servo.ino — risco n. 4 do flight-computer).

O sketch imprime uma linha CSV por tentativa:
    RUN,OPEN_MS,TRIPPED,OPEN_FAIL,HOLD_FAIL

Este script calcula estatisticas (media, desvio, pior caso) e da o veredito
contra os criterios de aprovacao (espelhados do sketch / README.md):
    - B: tempo comando -> abertura total < 1000 ms em TODAS as tentativas
    - C: porta permanece aberta (sem back-drive) em TODAS as tentativas
    - A: abertura completa em 20/20 tentativas (TRIPPED=1)

Uso:
    python3 analyze_servo_open_times.py log_servo.csv
    python3 analyze_servo_open_times.py < log_servo.csv   # via stdin

Exit code: 0 = PASS, 1 = FAIL (util para CI futura).

Nota: o criterio de corrente (D/E do README) e medido com multimetro e nao
aparece no CSV — preencha manualmente na tabela de resultados do README.
"""

import csv
import math
import sys

# ── Criterios (espelho de test/parachute_servo/parachute_servo.ino) ──────────
OPEN_TIME_LIMIT_MS = 1000  # criterio B: tempo maximo comando -> abertura total
REQUIRED_TRIALS = 20       # criterio A: numero de tentativas exigido
MIN_TRIPPED = 20           # criterio A: abertura completa em 20/20

# ── Cabeçalho do CSV emitido pelo sketch ──────────────────────────────────────
HEADER = ("RUN", "OPEN_MS", "TRIPPED", "OPEN_FAIL", "HOLD_FAIL")


def parse_rows(lines):
    """Converte as linhas do log em listas de tuplas (RUN, OPEN_MS, ...).

    Ignora comentarios (linhas com '#'), cabecalho e linhas vazias.
    """
    rows = []
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("RUN"):  # cabecalho do sketch
            continue
        if line.startswith(("STATS", "DONE", "SUMMARY")):
            continue
        try:
            run, open_ms, tripped, open_fail, hold_fail = line.split(",")
            rows.append(
                (
                    int(run),
                    int(open_ms),
                    int(tripped),
                    int(open_fail),
                    int(hold_fail),
                )
            )
        except ValueError:
            print(f"  [aviso] linha ignorada (formato inesperado): {line!r}")
    return rows


def analyze(rows):
    """Computa estatisticas e aplica os criterios de aprovacao."""
    n = len(rows)
    if n == 0:
        return None

    n_tripped = sum(1 for r in rows if r[2] == 1)
    n_open_fail = sum(1 for r in rows if r[3] == 1)
    n_hold_fail = sum(1 for r in rows if r[4] == 1)

    # Estatisticas sobre os tempos das tentativas que abriram de verdade.
    open_times = [r[1] for r in rows if r[2] == 1]
    if open_times:
        mean = sum(open_times) / len(open_times)
        var = sum((t - mean) ** 2 for t in open_times) / len(open_times)
        std = math.sqrt(var)
        worst = max(open_times)
    else:
        mean = std = worst = float("nan")

    # Veredito (criterios fixados ANTES do teste — nao afrouxar).
    pass_a = n >= REQUIRED_TRIALS and n_tripped >= MIN_TRIPPED
    pass_b = n_open_fail == 0
    pass_c = n_hold_fail == 0
    verdict = pass_a and pass_b and pass_c

    return {
        "n": n,
        "n_tripped": n_tripped,
        "n_open_fail": n_open_fail,
        "n_hold_fail": n_hold_fail,
        "mean_ms": mean,
        "std_ms": std,
        "worst_ms": worst,
        "pass_a": pass_a,
        "pass_b": pass_b,
        "pass_c": pass_c,
        "verdict": verdict,
    }


def report(stats):
    """Imprime o relatorio formatado."""
    if stats is None:
        print("Nenhuma tentativa encontrada no log.")
        return

    print("=" * 60)
    print("ANALISE — SERVO DO PARAQUEDAS SOB CARGA (test/parachute_servo)")
    print("=" * 60)
    print(f"  tentativas          : {stats['n']}  (exigido: {REQUIRED_TRIALS})")
    print(f"  aberturas completas : {stats['n_tripped']}  (exigido: {MIN_TRIPPED})")
    print(f"  falhas de abertura  : {stats['n_open_fail']}")
    print(f"  falhas de hold      : {stats['n_hold_fail']}")
    print("-" * 60)
    print(f"  tempo de abertura (ms)")
    print(f"    media     : {stats['mean_ms']:.1f}")
    print(f"    desvio    : {stats['std_ms']:.1f}")
    print(f"    pior caso : {stats['worst_ms']:.1f}")
    print("-" * 60)
    print(f"  [A] abertura completa 20/20 : {'PASS' if stats['pass_a'] else 'FAIL'}")
    print(f"  [B] tempo < {OPEN_TIME_LIMIT_MS} ms em todas   : {'PASS' if stats['pass_b'] else 'FAIL'}")
    print(f"  [C] hold (sem back-drive)   : {'PASS' if stats['pass_c'] else 'FAIL'}")
    print("-" * 60)
    print(f"  VEREDITO: {'PASS' if stats['verdict'] else 'FAIL'}")
    if not stats["verdict"]:
        print("  -> Veja 'O que fazer se falhar' no README.md do teste.")
    print("=" * 60)


def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1], "r", encoding="utf-8") as f:
            lines = f.readlines()
    else:
        lines = sys.stdin.readlines()

    stats = analyze(parse_rows(lines))
    report(stats)
    sys.exit(0 if (stats and stats["verdict"]) else 1)


if __name__ == "__main__":
    main()
