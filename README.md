# Volumetric Display Prototype

A swept-volume volumetric display built from scratch using a 
scotch yoke mechanism, custom SMT PCB, and Waveshare ESP32-S3-Matrix.

**Status:** Electronics and PCB tested ✅ | Mechanical assembly pending 🔄

> ⚠️ **Note:** This is an active prototype project. Thus, Hardware files and firmware are subject to rapid iterations.
---

## What This Is

A proof-of-concept volumetric display similar in basic principle to the 
Voxon VX1. An 8×8 RGB LED matrix sweeps vertically at ~16Hz 
using a scotch yoke mechanism, creating a 3D persistence-of-vision 
image visible from multiple angles.\
It is highly inspired from [Matthew Lim's video](https://youtu.be/KgT20tHpk1g?si=lmqNyyaOBYBv9zQp) on youtube about mini volumetric display.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | Waveshare ESP32-S3-Matrix (8×8 RGB LED matrix) |
| Custom PCB | 2-layer SMT, designed in Fusion 360, fabricated by Lion Circuits |
| Motor | GA12-N20 6V 1000RPM DC motor |
| Motor driver | DRV8838DSGR (on the PCB) |
| Position sensing | Dual ITR-9606 opto interrupters |
| Mechanism | Scotch yoke |
| PCB size | 27.5mm × 40mm |

---

## Firmware Architecture 

Core 0 (real-time):\
→ Motor PWM control via DRV8838\
→ Dual opto interrupt handling\
→ Slice timing calculation

Core 1 (non-critical):\
→ WiFi connection\
→ OTA wireless firmware updates\
→ Web debug dashboard

---

## Current Progress

- [DONE] Custom PCB designed, fabricated, assembled
- [DONE] Motor control verified (3 speeds via PWM)
- [DONE] Dual opto interrupter timing verified
- [DONE] Dual-core firmware with WiFi debug dashboard
- [DONE] OTA wireless firmware updates
- [PENDING] Mechanical assembly
- [PENDING] Volumetric slice display firmware

---

## Repository Structure

```text
├── hardware/
│   ├── pcb/               # Gerber Files, BOM, schematic, PCB photos
│   └── cad/        
│       ├── failed_design/     # First design + notes on why it failed
│       └── current_design/    # Current redesigned mechanism
├── firmware/              # Arduino IDE sketches
├── build_log/                  # Build log, images
└── references/            # Inspiration and related work
```
---

## PCB

Custom 2-layer controller board handling:
- USB-C power input with proper CC resistor negotiation(5.1k ohms resistors)
- DRV8838DSGR motor driver (1.8A capable)
- Dual opto interrupter interface circuits
- 7-pin interface to ESP32 board
- Status LED

Fabricated by Lion Circuits, India.

---

## Why **Scotch Yoke** Mechanism?

Unlike rack-and-pinion (used in Matthew Lim's build) and slider crank mechanism (which also used for converting circular motion into linear reciprocating motion), 
Scotch Yoke produces Simple Harmonic Motion:
- Screen dwells longer at extremes → better brightness distribution
- No backlash at direction reversal
- Smoother reversal → lower vibration
- Mathematically clean position from sin(angle)

---

## Build Log

See [build_log.md](build_log/build_log.md)

---

## Related Work

- [Matthew Lim's mini volumetric display](https://youtu.be/KgT20tHpk1g?si=lmqNyyaOBYBv9zQp)
- [Voxon Photonics VX1/VX2](https://voxon.co)

---

## Author

Gandhi Kalp — 2nd year B.Tech Civil Engineering, IIT ISM Dhanbad  
Interests: Robotics, Embedded Systems, IoT, ML and Automation

Check my [LinkedIn](https://www.linkedin.com/in/gandhikalpniravkumar/?skipRedirect=true)