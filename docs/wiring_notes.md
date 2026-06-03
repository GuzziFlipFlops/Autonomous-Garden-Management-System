# Wiring Notes

These notes are a working checklist for wiring the robot safely and maintainably.

## Power

- Use a main fuse or breaker close to the battery.
- Use wire gauges rated for expected current.
- Keep mower power, drivetrain power, and logic power clearly labeled.
- Use a main disconnect that can be reached quickly during testing.

## Signal Wiring

- Keep low-voltage signal wiring away from high-current motor wiring where practical.
- Use signal isolation/protection where practical.
- Label encoder, PWM, IMU, and controller connections.
- Add strain relief wherever wires move with the chassis or turret.

## Grounding

- Verify common grounds where required.
- Verify isolation boundaries where isolation is used.
- Check polarity before powering any controller.

## Documentation Needed

- Final control box wiring diagram
- STM32 pin maps
- ESP32-P4 pin map
- Motor driver wiring diagram
- Mower controller wiring diagram
- Hose turret wiring diagram
- Fuse and disconnect locations
