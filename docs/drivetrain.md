# Drivetrain

The drivetrain uses DS3240 servos repurposed as drivetrain motors. This is much cheaper than many traditional robotics drivetrain motor options and supports the low-cost platform goal.

## Current Status

The drivetrain is mechanically done. Electrical and firmware work are still in progress.

## Control Plan

The low-level STM32 F103 boards are planned to handle:

- Encoder reading
- Motor PWM generation
- BTS7960 motor driver control
- Sensor I/O related to drivetrain behavior

The ESP32-P4 will coordinate higher-level movement commands once the low-level drivetrain control is stable.

## Testing Needed

- Low-speed indoor or bench drivetrain tests with mower disabled
- Encoder direction checks
- Motor driver thermal checks
- Straight-line movement tests
- Turning tests
- Grass and uneven-terrain tests
- Emergency stop behavior
