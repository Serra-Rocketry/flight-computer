# Telemetry Format (v2.0)

Single source of truth for the LoRa telemetry protocol between the flight
computer (`flight-computer`) and the ground receiver (`recovery-webui/
components/receiver-lora`).

Defined in Fase 10 of `firmware/REFACTORING_PLAN.md`. Any change here MUST be
reflected in:
- `firmware/flight/TelemetryTask.cpp` (`assembleTelemetry`)
- `firmware/flight/TelemetryTask.cpp` (CSV header in `initTelemetryTask`)
- `recovery-webui/components/receiver-lora/firmware/src/main.cpp`
  (`parseSatellitePacket`, `buildProtocolPacket`)
- `recovery-webui/components/receiver-lora/firmware/include/payload.h`
- `recovery-webui/components/receiver-lora/firmware/arduino/receiver-lora/
  receiver-lora.ino` (legacy mirror)

---

## 1. Radio parameters (must match on both ends)

| Parameter     | Value       | Note                                  |
|---------------|-------------|---------------------------------------|
| Frequency     | 915 MHz     | `LORA_FREQ = 915E6` (Brazil/Americas) |
| Sync word     | 0xF3        | `SYNC_WORD`                           |
| Spreading F.  | 7           | `LORA_SF`                             |
| Bandwidth     | 125 kHz     | `LORA_BW = 125E3`                     |
| Coding rate   | 4/5         | `LORA_CR = 5`                         |
| TX power      | 17 dBm      | `LORA_TX_POWER`                       |
| CRC           | enabled     | `LoRa.enableCrc()`                    |

> Pins (SS/RST/DIO0) are per-board and need NOT match.

---

## 2. Satellite -> Receiver packet (22 fields, CSV)

Emitted by the flight computer (`assembleTelemetry`) at ~5 Hz over LoRa and
to the on-board LittleFS CSV.

| #  | Field        | Type    | Unit     | Description                          |
|----|--------------|---------|----------|--------------------------------------|
| 0  | TEAM_ID      | string  | -        | Team identifier (e.g. `#213`)        |
| 1  | millis       | uint32  | ms       | `millis()` since boot                |
| 2  | count        | uint32  | -        | Sequential packet counter            |
| 3  | altp         | float   | m        | Barometric altitude                  |
| 4  | temp         | float   | C        | Temperature                          |
| 5  | umi          | float   | %        | Humidity — `0` (no humidity sensor)  |
| 6  | p            | float   | hPa      | Pressure                             |
| 7  | gx           | float   | rad/s    | Gyro X                               |
| 8  | gy           | float   | rad/s    | Gyro Y                               |
| 9  | gz           | float   | rad/s    | Gyro Z                               |
| 10 | ax           | float   | m/s²     | Accel X                              |
| 11 | ay           | float   | m/s²     | Accel Y                              |
| 12 | az           | float   | m/s²     | Accel Z                              |
| 13 | vz           | float   | m/s      | Vertical velocity                    |
| 14 | maxAltitude  | float   | m        | Max altitude so far                  |
| 15 | state        | int     | -        | Flight state machine state (0-6)     |
| 16 | alt          | float   | m        | GPS altitude (`nan` if no fix)       |
| 17 | lat          | float   | deg      | GPS latitude (`nan` if no fix)       |
| 18 | lon          | float   | deg      | GPS longitude (`nan` if no fix)      |
| 19 | sat          | uint8   | -        | GPS satellite count                  |
| 20 | parachute    | int     | 0/1      | Parachute deployed flag              |
| 21 | rssi         | int     | dBm      | **Placeholder 0** — receiver overrides|

> The `rssi` field sent by the satellite is always `0`. The receiver measures
> the real downlink RSSI (`LoRa.packetRssi()`) and substitutes it in the
> protocol packet below.

---

## 3. Receiver -> Recovery WebUI packet (24 fields, CSV)

Retransmitted by the receiver over USB Serial and saved to its LittleFS.
Adds local GPS `hora`/`data` and the real `rssi`.

| #  | Field        | From                                     |
|----|--------------|------------------------------------------|
| 0  | TEAM_ID      | satellite field 0                        |
| 1  | millis       | satellite field 1                        |
| 2  | count        | satellite field 2                        |
| 3  | altp         | satellite field 3                        |
| 4  | temp         | satellite field 4                        |
| 5  | umi          | satellite field 5                        |
| 6  | p            | satellite field 6                        |
| 7  | gx           | satellite field 7                        |
| 8  | gy           | satellite field 8                        |
| 9  | gz           | satellite field 9                        |
| 10 | ax           | satellite field 10                       |
| 11 | ay           | satellite field 11                       |
| 12 | az           | satellite field 12                       |
| 13 | vz           | satellite field 13                       |
| 14 | maxAltitude  | satellite field 14                       |
| 15 | state        | satellite field 15                       |
| 16 | hora         | **receiver GPS** (HHMMSS)                |
| 17 | data         | **receiver GPS** (DDMMYYYY)              |
| 18 | alt          | satellite field 16                       |
| 19 | lat          | satellite field 17                       |
| 20 | lon          | satellite field 18                       |
| 21 | sat          | satellite field 19                       |
| 22 | parachute    | satellite field 20                       |
| 23 | rssi         | **receiver** `LoRa.packetRssi()`         |

The LittleFS log of the receiver prepends its own `millis()` as column 0:
`millis,TEAM_ID,millis_ts,count,...,parachute,rssi`.
