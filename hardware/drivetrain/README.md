# Drivetrain Hardware

The drivetrain is mechanically done and uses DS3240 servos repurposed as drivetrain motors.

## Why DS3240 Servos

DS3240 servos are much cheaper than many traditional robotics drivetrain motor options. Repurposing them keeps the platform cost low while still giving the robot useful torque for early testing.

## Control Hardware

Planned low-level control uses STM32 F103 Blue Pill boards for encoder reading, motor PWM, BTS7960 control, and related sensor I/O.

## Testing Needed

- Encoder checks
- Motor direction checks
- Low-speed driving
- Driver thermal checks
- Outdoor grass testing
