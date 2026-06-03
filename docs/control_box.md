# Control Box

The control box is intended to organize and protect the robot electronics during outdoor testing.

## Planned Contents

- ESP32-P4 high-level controller
- Two STM32 F103 Blue Pill low-level controllers
- Motor driver wiring
- Power distribution
- Logic voltage regulation
- Signal isolation/protection
- Sensor wiring terminals
- Safety disconnects
- Debug and programming access

## Design Goals

- Keep high-current motor wiring separated from low-voltage logic wiring where practical.
- Make power and signal routing understandable.
- Provide strain relief for vibration and outdoor movement.
- Make it easy to disconnect mower power during drivetrain-only tests.
- Keep debug access available without exposing unnecessary wiring.

## Current Status

The control box layout is still in progress.
