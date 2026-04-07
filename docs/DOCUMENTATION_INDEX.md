# Flight Computer v2.0 - Documentation Index

**Last Updated**: 2026-04-06  
**Project**: Flight Computer (Team #100, ESP32-S3)  
**Version**: 2.0.0-dev (Phases 1-2 Complete)

---

## Quick Navigation

### For New Team Members
1. Start with: [README.md](../README.md) - Project overview
2. Read: [Phase 1-2 Summary](./phase-1-2-summary.md) - What was completed
3. Study: [ISensor Interface](../firmware/sensors/ISensor.h) - Core abstraction

### For Architects
1. [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md) - Complete v2.0 specification
2. [ADR 002 - Sensor Abstraction](./adr/002-sensor-abstraction.md) - Design decisions
3. [Software Architecture](./software.md) - Module structure & diagrams

### For Developers
1. [Software.md](./software.md) - Component reference
2. [ISensor.h](../firmware/sensors/ISensor.h) - Interface contracts
3. [SensorData.h](../firmware/flight/SensorData.h) - Data structures
4. [Refactoring Plan](../firmware/REFACTORING_PLAN.md) - Implementation details

### For Reviewers
1. [Safety Review](../SAFETY_REVIEW_PHASE_1_2.md) - Code review findings
2. [Phase 1-2 Analysis](./ANALISE_FASES_1_2.md) - Detailed analysis
3. [CHANGELOG.md](../CHANGELOG.md) - What changed

---

## Documentation Structure

```
flight-computer/
├── README.md                           # Project overview
├── CONTRIBUTING.md                     # Contribution guidelines
├── CHANGELOG.md                        # Version history (UPDATED)
│
├── docs/
│   ├── DOCUMENTATION_INDEX.md          # This file (navigation guide)
│   ├── software.md                     # Software architecture (UPDATED)
│   ├── hardware.md                     # Hardware specifications
│   ├── flowchart.md                    # System flowcharts
│   │
│   ├── phase-1-2-summary.md            # ⭐ NEW - Phase 1-2 completion
│   ├── ANALISE_FASES_1_2.md            # Detailed analysis
│   ├── ANALISE_FASES_1_2_RESUMO.txt    # Analysis summary
│   │
│   ├── adr/                            # Architecture Decision Records
│   │   └── 002-sensor-abstraction.md   # ⭐ NEW - ISensor design
│   │
│   └── CDB.png                         # Block diagram
│
├── firmware/
│   ├── REFACTORING_PLAN.md             # v2.0 complete specification
│   ├── MODULOS.md                      # Module documentation
│   │
│   ├── sensors/                        # ⭐ NEW - Sensor abstraction
│   │   └── ISensor.h                   # Abstract sensor interface
│   │
│   ├── flight/                         # ⭐ NEW - Flight control
│   │   └── SensorData.h                # Data structures
│   │
│   ├── modules/                        # Refactored modules
│   │   ├── TelemetryTask.h
│   │   ├── LoggerTask.h
│   │   └── ...
│   │
│   └── firmware.ino                    # Main (v1.0 legacy)
│
├── SAFETY_REVIEW_PHASE_1_2.md          # ⭐ NEW - Code review
├── hardware/                           # Hardware files
├── test/                               # Unit tests
└── extras/                             # Reference code
```

---

## Phase Status Summary

### ✅ Phase 1-2: COMPLETE
**Date**: 2026-04-06  
**Deliverables**: ISensor interface, SensorData structures, documentation

**Key Files**:
- [phase-1-2-summary.md](./phase-1-2-summary.md) - Executive summary
- [ISensor.h](../firmware/sensors/ISensor.h) - Interface definition
- [SensorData.h](../firmware/flight/SensorData.h) - Data structures
- [002-sensor-abstraction.md](./adr/002-sensor-abstraction.md) - Architecture decision

### ⏳ Phase 3: IN PROGRESS
**Objective**: Sensor implementations  
**Target Date**: 2026-04-10

**Expected Deliverables**:
- BMP585Sensor.h - Barometric sensor
- LSM6DS3Sensor.h - IMU sensor
- GPSModule.h - GNSS positioning

### 📋 Phase 4+: PLANNED
**Phase 4**: Flight control integration  
**Phase 5**: Communication & logging  
**Phase 6**: Sensor redundancy  
**Phase 7**: Hardware integration testing

---

## Document Purposes & Contents

### High-Level Overview

| Document | Purpose | Audience | Length |
|----------|---------|----------|--------|
| [README.md](../README.md) | Project introduction | Everyone | ~200 lines |
| [Phase 1-2 Summary](./phase-1-2-summary.md) | Completion status & metrics | Project leads | 554 lines |
| [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md) | Complete specification | Architects | 400+ lines |

### Architecture & Design

| Document | Purpose | Audience | Status |
|----------|---------|----------|--------|
| [ADR 002 - Sensor Abstraction](./adr/002-sensor-abstraction.md) | Design decision for ISensor | Architects | ✅ Complete |
| [Software Architecture](./software.md) | Module structure & diagrams | Developers | ✅ Updated |
| [Hardware Specs](./hardware.md) | Component specifications | Hardware engineers | ✅ Maintained |

### Implementation Guides

| Document | Purpose | Use Case | Status |
|----------|---------|----------|--------|
| [ISensor.h](../firmware/sensors/ISensor.h) | Interface contracts | Implementing sensors | ✅ Complete |
| [SensorData.h](../firmware/flight/SensorData.h) | Data structures | Using sensor data | ✅ Complete |
| [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md) | Implementation details | All phases | ✅ Complete |

### Analysis & Review

| Document | Purpose | Detail Level | Status |
|----------|---------|--------------|--------|
| [Safety Review](../SAFETY_REVIEW_PHASE_1_2.md) | Code quality assessment | Critical findings | ✅ Complete |
| [ANALISE_FASES_1_2.md](./ANALISE_FASES_1_2.md) | Detailed architecture review | Comprehensive | ✅ Complete |
| [CHANGELOG.md](../CHANGELOG.md) | Version history & changes | Summary | ✅ Updated |

---

## Key Sections by Role

### 👨‍💼 Project Manager
- [Phase 1-2 Summary](./phase-1-2-summary.md) - Project status
- [CHANGELOG.md](../CHANGELOG.md) - Change log
- Metrics section in summary

### 🏗️ Software Architect
- [ADR 002 - Sensor Abstraction](./adr/002-sensor-abstraction.md) - Design rationale
- [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md) - Full specification
- [Software Architecture](./software.md) - Module structure

### 👨‍💻 Firmware Developer
- [ISensor.h](../firmware/sensors/ISensor.h) - Interface to implement
- [SensorData.h](../firmware/flight/SensorData.h) - Data structures to use
- [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md) - Implementation steps
- [Software Architecture](./software.md) - Component reference

### 🔍 Code Reviewer
- [Safety Review](../SAFETY_REVIEW_PHASE_1_2.md) - Critical findings
- [ANALISE_FASES_1_2.md](./ANALISE_FASES_1_2.md) - Detailed analysis
- [ISensor.h](../firmware/sensors/ISensor.h) & [SensorData.h](../firmware/flight/SensorData.h) - Source code

### 🧪 Test Engineer
- [Phase 1-2 Summary](./phase-1-2-summary.md) - Testing section (Phase 3 plan)
- [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md) - Validation steps
- [ISensor.h](../firmware/sensors/ISensor.h) - Mock implementation hints

---

## Recent Changes (Phase 1-2)

### New Documents (Created 2026-04-06)

1. **docs/phase-1-2-summary.md** (554 lines)
   - Executive summary of Phases 1-2
   - Checkpoint metrics and RAM budget
   - Completion status and roadmap

2. **docs/adr/002-sensor-abstraction.md** (321 lines)
   - Architecture Decision Record
   - Rationale for ISensor interface
   - Design patterns and consequences

3. **SAFETY_REVIEW_PHASE_1_2.md** (502 lines)
   - Code review by safety-critical specialist
   - Critical findings and fixes
   - Validation checklist

### Updated Documents

4. **CHANGELOG.md** (109 lines)
   - Added Phase 1-2 entries in [Unreleased] section
   - Documented safety initializations
   - Added commit references

5. **docs/software.md** (417 lines)
   - New FreeRTOS architecture diagram
   - Updated project structure
   - Phase status table
   - ISensor → Implementation mapping

### Existing Documents Maintained

- ✅ REFACTORING_PLAN.md - Complete specification
- ✅ ANALISE_FASES_1_2.md - Detailed analysis
- ✅ firmware/sensors/ISensor.h - Interface definition
- ✅ firmware/flight/SensorData.h - Data structures

---

## Quick Reference: File Locations

### Core Interfaces
- **ISensor**: `firmware/sensors/ISensor.h`
- **SensorData**: `firmware/flight/SensorData.h`
- **FlightState**: Defined in SensorData.h

### Documentation
- **Architecture**: docs/adr/002-sensor-abstraction.md
- **Specification**: firmware/REFACTORING_PLAN.md
- **Software**: docs/software.md
- **Safety Review**: SAFETY_REVIEW_PHASE_1_2.md
- **Summary**: docs/phase-1-2-summary.md

### Configuration
- **Constants**: firmware/config.h
- **Hardware Pins**: See docs/hardware.md

### Tests
- **Unit Tests**: test/ directory
- **Integration Tests**: extras/FSM_tester/

---

## How to Use This Index

1. **Finding Information**:
   - Use the "Quick Navigation" section at the top
   - Search by role in "Key Sections by Role"
   - Check "Document Purposes & Contents" table

2. **Understanding Progress**:
   - Read "Phase Status Summary"
   - Check [phase-1-2-summary.md](./phase-1-2-summary.md) for metrics

3. **Implementing Phase 3**:
   - Review [REFACTORING_PLAN.md](../firmware/REFACTORING_PLAN.md)
   - Study [ISensor.h](../firmware/sensors/ISensor.h) examples
   - Follow [Phase 3 section](./phase-1-2-summary.md#next-steps-phase-3---sensor-implementations)

4. **Making Architecture Decisions**:
   - Read [ADR 002](./adr/002-sensor-abstraction.md)
   - Review consequences and tradeoffs
   - Propose new ADRs for major changes

---

## Contributing New Documentation

When adding new documentation:

1. **Follow Naming Conventions**:
   - Architecture Decision Records: `adr/NNN-decision-title.md`
   - Phase Summaries: `phase-N-summary.md`
   - Analysis Documents: Descriptive names

2. **Update This Index**:
   - Add entry to relevant table
   - Update "Recent Changes" section
   - Add to directory tree (if new location)

3. **Maintain Standards**:
   - Use Markdown format (.md)
   - Include Date and Version
   - Add status badges (✅ ⏳ 📋)
   - Link to related documents

---

## Document Quality Metrics (Phase 1-2)

| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| **Doxygen Coverage** | 100% | 100% | ✅ |
| **ADR Completeness** | >300 lines | 321 lines | ✅ |
| **Phase Summary** | >500 lines | 554 lines | ✅ |
| **CHANGELOG Updates** | Yes | Yes | ✅ |
| **Diagrams** | Mermaid | 2 diagrams | ✅ |
| **Cross-References** | Complete | Complete | ✅ |

---

## Support & Questions

### For Documentation Issues
- File a GitHub issue with tag `[docs]`
- Reference specific document and line number
- Include suggestion for improvement

### For Architecture Questions
- Reference relevant ADR (docs/adr/)
- Check REFACTORING_PLAN.md first
- Post on project discussion board

### For Implementation Help
- Check Phase 1-2 Summary for roadmap
- Review ISensor.h for interface contracts
- Study existing sensor implementations (Phase 3)

---

## Version History

| Date | Version | Changes | Author |
|------|---------|---------|--------|
| 2026-04-06 | 1.0 | Initial documentation index created | Doc Specialist |
| - | - | Phase 1-2 completion documented | - |
| - | - | 5 new/updated documents | - |

---

**Status**: ✅ **DOCUMENTATION COMPLETE FOR PHASES 1-2**

For Phases 3+ updates, see [Phase 1-2 Summary](./phase-1-2-summary.md#next-steps-phase-3---sensor-implementations)
