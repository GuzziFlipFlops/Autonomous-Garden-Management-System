# Roadmap

## Phase 1 — Mechanical Platform

- Build aluminum-channel chassis
- Complete drivetrain
- Complete front mower assembly
- Complete mower lift mechanism
- Mount main electronics

Status: mostly complete

## Phase 2 — Low-Level Control

- Wire drivetrain encoders
- Test STM32 F103 analog encoder reading
- Test BTS7960 motor control
- Send low-level motor/sensor data to ESP32-P4
- Validate drivetrain movement feedback

Status: in progress

## Phase 3 — Mower Control

- Test BLDC mower motor control
- Add safe enable/disable logic
- Add emergency cutoff
- Test mower lift control
- Document safe operating limits

Status: in progress

## Phase 4 — Rear Turret

- Complete worm-gear rotating base
- Add second axis for hose elevation
- Test turret repeatability
- Add hose mount
- Add base-station docking interface

Status: in progress

## Phase 5 — Base Station and Watering

- Build hose docking station
- Add electrically controlled water flow
- Add docking alignment sensors
- Test attach/detach behavior
- Test hose pointing accuracy

Status: planned

## Phase 6 — Autonomous Operation

- Add navigation sensors
- Add basic path following
- Add obstacle detection
- Add docking behavior
- Add plant-targeted watering behavior

Status: planned

## Phase 7 — Field Testing

- Test on concrete
- Test on grass
- Test slope handling
- Test mower height system
- Test watering turret
- Measure runtime, reliability, and thermal behavior

Status: planned
