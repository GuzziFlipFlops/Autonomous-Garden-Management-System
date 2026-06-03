# Electronics Architecture

GMS uses a distributed microcontroller architecture to keep cost low while still separating high-level behavior from low-level real-time I/O.

## High-Level Controller

The ESP32-P4 is the planned high-level controller. It is used instead of an expensive Raspberry Pi 5-class SBC.

Planned responsibilities:

- Coordinate subsystem states
- Manage designed autonomous operation
- Communicate with low-level controllers
- Coordinate mower, lift, drivetrain, turret, and docking behavior
- Log useful test data where practical

## Low-Level Controllers

Two STM32 F103 Blue Pill boards are planned for low-level I/O.

Planned responsibilities:

- Encoder reading
- Motor PWM
- BTS7960 control
- Sensor I/O
- Timing-sensitive subsystem control

## Sensors and Motor Control

- BNO085 IMU for orientation and motion sensing
- BTS7960 modules for brushed drivetrain motor control
- Low-cost ODrive-style FOC controller for the BLDC mower motor
- Turret motor control to be finalized as the rear module develops

## Power

The robot uses a 36V main power system. Logic electronics require regulated lower-voltage rails and should be protected from noisy motor power where practical.
