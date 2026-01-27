# Avionics - Onboard Computer

![Version](https://img.shields.io/badge/tag-v1.0.0--LASC2025-orange)
![Date](https://img.shields.io/badge/released-Jan%2027%202026-lightgrey)
![Status](https://img.shields.io/badge/status-V2%20In%20Development-yellow)

> 🚧 **Development Notice**
>
> The V1 flight computer architecture has reached **End of Life (EOL)** and is preserved for historical reference.
>  
> Active development is ongoing for **V2**, featuring a new architecture based on **FreeRTOS and Finite State Machines**, on the `dev-2026` branch.

Onboard computer (OBC) for the SR21000 rocket of the Serra Rocketry rocket modeling team. An embedded system that monitors altitude, velocity and GPS position, and autonomously controls parachute deployment during flight.

## Overview

- Real-time monitoring of altitude, velocity and GPS position
- LoRa communication with the operational base
- Telemetry storage in CSV format
- Automatic servo control for parachute opening
- Web interface for accessing stored data

## Directory Structure

```
firmware/       Main onboard computer code
test/           Unit tests of individual components
extras/         Support code and experimentation
hardware/       Schematics, PCB and component list
docs/           Detailed documentation (software and hardware)
```

## Documentation

- **[software.md](docs/software.md)** - Firmware details, functions and libraries
- **[hardware.md](docs/hardware.md)** - Components, pinout and specifications
- **[flowchart.md](docs/flowchart.md)** - System execution flowchart

## Getting Started

> ⚠️ This section applies only to the V1 (LASC 2025) firmware.
>  
> The V2 system uses a different architecture and toolchain.

1. Open [firmware/firmware.ino](firmware/firmware.ino) in Arduino IDE
2. Install required libraries
3. Configure COM port and ESP32 board
4. Upload to microcontroller

## Repository Structure

- **`main`**  
  Legacy V1 flight computer (LASC 2025) – frozen and released.

- **`dev-2026`**  
  Active development branch for the V2 flight computer architecture.

- **Releases / Tags**  
  - `v1.0.0-LASC2025`: Final V1 flight firmware (SR21000 mission).

## License

Project by Serra Rocketry - Rocket Modeling Team at IPRJ/UERJ
