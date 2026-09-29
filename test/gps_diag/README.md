# GPS Raw Sniffer (`test/gps_diag/`)

One-shot diagnostic that finds the GPS UART wiring without any assumptions:
sniffs the two candidate pins raw (RX-only, never drives the GPS lines) at
4800/9600/38400/115200, in both orientations, and prints ASCII runs so NMEA
sentences are visible even without full sync.

## Why it exists (2026-08-27 finding)

The bench bench-test (`test/bench`, command `g`) reported 0 bytes from the GPS
at every baud, yet the module's LED blinked (alive and tracking). The raw
sniffer proved the **GPS TX line physically arrives on GPIO17**, not GPIO18 as
the schematic comment in `config.h` claimed:

- RX=GPIO17 @9600: 1468 bytes, clean NMEA (`$GNGGA`, `$GNRMC`, ...), 3D fix,
  10 satellites, Rio de Janeiro position.
- RX=GPIO18: 0 bytes at every baud.

`firmware/config.h` was swapped to `RX_GPS=17 / TX_GPS=18` (documented as
bench-measured) and the bench GPS test went 4/4 PASS (11 sats).

## Flash & run

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 test/gps_diag
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3 test/gps_diag
# Serial monitor at 115200 — runs once at boot and prints the full matrix
```

## Reading the output

- The winning combo = the row with **NMEA text** (`$GN...`, `$GP...`) at a
  sane byte rate. Neighbouring bauds of the same pin will show garbage
  ("xxxx" runs) — that is framing mismatch, not a second device.
- If BOTH pins are dead at all bauds: check GPS power (LED), then continuity
  of the TX line to the ESP32 header.

## Lesson

When schematic and hardware disagree, trust the raw sniffer. This is the
second schematic-vs-reality divergence found on this board (LoRa SPI pinout
was the first). The schematic comment blocks in `config.h` are intentions,
not measurements.
