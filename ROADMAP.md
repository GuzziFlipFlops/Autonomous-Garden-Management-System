# Roadmap

This roadmap reflects the current public build plan. Dates are intentionally omitted until each subsystem has enough test data to support scheduling.

## Phase 1 - Mechanical Platform

Status: mostly complete

- Build aluminum-channel chassis
- Complete drivetrain mechanical assembly
- Complete front mower module
- Complete rope-based mower lift mechanism
- Mount major front-section hardware

## Phase 2 - Low-Level Control

Status: in progress

- Wire drivetrain encoders
- Test STM32 F103 encoder reading
- Test BTS7960 motor control
- Send low-level motor and sensor data to the ESP32-P4
- Validate drivetrain movement feedback

## Phase 3 - Mower Control

Status: in progress

- Test C6374-class BLDC mower motor control
- Validate low-cost ODrive-style FOC controller behavior
- Add safe enable and disable logic
- Add emergency cutoff behavior
- Test mower lift control under load
- Document safe operating limits

## Phase 4 - Rear Hose Turret

Status: in progress

- Complete worm-gear rotating base
- Test first turret degree of freedom
- Build second hose elevation axis
- Add hose mount
- Add base-station hose interface

## Phase 5 - Base Station and Watering

Status: planned

- Build hose docking station
- Add controlled water flow
- Add docking alignment sensors or guides
- Test autonomous hose attach/detach behavior
- Test hose pointing repeatability

## Phase 6 - Designed Autonomous Operation

Status: planned

- Add navigation sensors as needed
- Add basic path following
- Add obstacle detection and stop behavior
- Add docking behavior
- Add plant-targeted watering behavior
- Keep mower-disabled tests separate from mower-enabled tests

## Phase 7 - Field Testing

Status: planned

- Test on flat ground
- Test on grass
- Test slope handling
- Test mower height repeatability
- Test watering turret aiming
- Measure runtime, reliability, and thermal behavior
- Update documentation after failures and design changes
