# System Overview

The Autonomous Garden Management System is a low-cost garden maintenance robot designed for autonomous operation. Its public purpose is mowing, watering, hose aiming, and modular garden tooling.

The project is still being worked on. The current repository documents the build as it develops rather than presenting a finished product.

## Major Subsystems

- Aluminum-channel chassis
- DS3240-servo-based drivetrain
- Front mower module
- Rope-based mower lift
- 36V electrical system
- ESP32-P4 high-level controller
- Two STM32 F103 Blue Pill low-level controllers
- BNO085 IMU
- Low-cost ODrive-style FOC controller for the mower motor
- Rear hose turret and base-station hose interface
- Optional controlled liquid application module, tested with water first

## Control Split

The ESP32-P4 is planned as the high-level controller. It coordinates robot behavior, subsystem states, docking behavior, and garden-tool actions.

The STM32 F103 boards are planned as low-level controllers for time-sensitive I/O such as encoder reading, motor PWM, BTS7960 control, and sensor input.

## Build Philosophy

The design favors parts that are affordable, easy to source, and practical to modify. The target core platform cost is around $250, but this will remain a target until the BOM is complete.
