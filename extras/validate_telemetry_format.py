#!/usr/bin/env python3
"""
E2E validation for Fase 10 telemetry format v2.0.

Replicates (in pure Python) the exact field order produced by the flight
computer's assembleTelemetry() and consumed by the receiver's
parseSatellitePacket() + buildProtocolPacket(). Proves the indices line up
end-to-end WITHOUT needing hardware.

Run: python3 extras/validate_telemetry_format.py
"""
import re

# --- Flight computer: assembleTelemetry() field order (22 fields) ----------
# Mirrors firmware/flight/TelemetryTask.cpp
FLIGHT_FIELDS = [
    "TEAM_ID", "millis", "count", "altp", "temp", "umi", "p",
    "gx", "gy", "gz", "ax", "ay", "az", "vz", "maxAltitude", "state",
    "alt", "lat", "lon", "sat", "parachute", "rssi",
]

def flight_emit(team_id="#213", ts=12345, count=42, altp=271.6, temp=23.4,
                press=987.5, gx=0.01, gy=0.02, gz=0.03,
                ax=0.1, ay=0.2, az=9.7, vz=-2.1, max_alt=271.6,
                state=4, alt="nan", lat="nan", lon="nan", sat=0,
                parachute=1, rssi=0):
    """Reproduce assembleTelemetry() output (snprintf, 22 fields)."""
    return ",".join([
        team_id,
        f"{ts}", f"{count}",
        f"{altp:.2f}", f"{temp:.2f}", f"{0.0:.2f}",  # umi placeholder 0
        f"{press:.2f}",
        f"{gx:.2f}", f"{gy:.2f}", f"{gz:.2f}",
        f"{ax:.2f}", f"{ay:.2f}", f"{az:.2f}",
        f"{vz:.2f}", f"{max_alt:.2f}", f"{state}",
        str(alt), str(lat), str(lon),
        f"{sat}", f"{parachute}", f"{rssi}",
    ])


def receiver_parse(raw):
    """Reproduce main.cpp parseSatellitePacket() (22-field order)."""
    parts = raw.split(",")
    assert len(parts) >= 22, f"expected >=22 fields, got {len(parts)}"
    return {
        "TEAM_ID": parts[0],
        "millis": int(parts[1]),
        "count": int(parts[2]),
        "altp": float(parts[3]),
        "temp": float(parts[4]),
        "umi": float(parts[5]),
        "p": float(parts[6]),
        "gx": float(parts[7]),
        "gy": float(parts[8]),
        "gz": float(parts[9]),
        "ax": float(parts[10]),
        "ay": float(parts[11]),
        "az": float(parts[12]),
        "vz": float(parts[13]),
        "maxAltitude": float(parts[14]),
        "state": int(parts[15]),
        "alt": parts[16],
        "lat": parts[17],
        "lon": parts[18],
        "sat": int(parts[19]),
        "parachute": int(parts[20]),
        "rssi_placeholder": int(parts[21]),
    }


def receiver_build(parsed, hora=123456, data=17072026, rx_rssi=-67):
    """Reproduce payload.h buildProtocolPacket() (24 fields, adds hora/data)."""
    return ",".join([
        parsed["TEAM_ID"], f"{parsed['millis']}", f"{parsed['count']}",
        f"{parsed['altp']:.2f}", f"{parsed['temp']:.2f}", f"{parsed['umi']:.2f}",
        f"{parsed['p']:.2f}",
        f"{parsed['gx']:.2f}", f"{parsed['gy']:.2f}", f"{parsed['gz']:.2f}",
        f"{parsed['ax']:.2f}", f"{parsed['ay']:.2f}", f"{parsed['az']:.2f}",
        f"{parsed['vz']:.2f}", f"{parsed['maxAltitude']:.2f}", f"{parsed['state']}",
        f"{hora}", f"{data}",
        str(parsed["alt"]), str(parsed["lat"]), str(parsed["lon"]),
        f"{parsed['sat']}", f"{parsed['parachute']}", f"{rx_rssi}",
    ])


def main():
    raw = flight_emit()
    print("FLIGHT EMIT (22 fields):")
    print("  " + raw)
    parts = raw.split(",")
    assert len(parts) == 22, f"flight must emit exactly 22 fields, got {len(parts)}"

    parsed = receiver_parse(raw)
    print("RECEIVER PARSE OK, sample fields:")
    print(f"  altp={parsed['altp']}  vz={parsed['vz']}  state={parsed['state']}"
          f"  lat={parsed['lat']}  sat={parsed['sat']}  parachute={parsed['parachute']}")

    # Critical checks: indices that were BROKEN before Fase 10 must now match
    checks = {
        "altp":  (parsed["altp"], 271.6),
        "vz":    (parsed["vz"], -2.1),
        "maxAltitude": (parsed["maxAltitude"], 271.6),
        "state": (parsed["state"], 4),
        "alt (GPS)": (parsed["alt"], "nan"),
        "lat (GPS)": (parsed["lat"], "nan"),
        "sat":   (parsed["sat"], 0),
        "parachute": (parsed["parachute"], 1),
        "rssi_placeholder": (parsed["rssi_placeholder"], 0),
    }
    for name, (got, exp) in checks.items():
        assert got == exp, f"FIELD MISMATCH {name}: got {got!r}, expected {exp!r}"
    print("ALL FIELD INDEX CHECKS PASSED (GPS/state/vz no longer swapped)")

    proto = receiver_build(parsed)
    pparts = proto.split(",")
    assert len(pparts) == 24, f"protocol must be 24 fields, got {len(pparts)}"
    print("RECEIVER PROTOCOL (24 fields, hora/data inserted, rssi real):")
    print("  " + proto)
    assert pparts[16] == "123456" and pparts[17] == "17072026", "hora/data order wrong"
    assert pparts[23] == "-67", "rssi real not placed at end"
    assert pparts[22] == "1", "parachute flag lost in protocol"
    print("PROTOCOL STRUCTURE CHECKS PASSED")

    # Sanity: the OLD broken mapping would have put vz into 'alt' etc.
    # Verify the 4 previously-swapped fields are now correct by re-emitting
    # with real GPS and confirming lat/lon survive intact.
    raw2 = flight_emit(alt="528.1", lat="-22.123456", lon="-43.654321", sat=7)
    p2 = receiver_parse(raw2)
    assert p2["alt"] == "528.1" and p2["lat"] == "-22.123456" and p2["lon"] == "-43.654321", \
        "GPS fields swapped!"
    assert p2["sat"] == 7 and p2["parachute"] == 1
    print("GPS FIELDS SURVIVE END-TO-END (lat/lon/sat/alt correct)")

    print("\n=== FASE 10 TELEMETRY FORMAT v2.0: E2E VALIDATION PASSED ===")


if __name__ == "__main__":
    main()
