# Firmware

Firmware is not yet included in this repository.

The planned architecture uses an ESP32-P4 as the high-level controller and two STM32 F103 Blue Pill boards as low-level controllers.

## Planned Folders

- [esp32_p4_high_level](esp32_p4_high_level/README.md): autonomy coordination and subsystem state management
- [esp32_tank_drive_odesc](esp32_tank_drive_odesc/README.md): ESP32 web GUI for BTS7960 tank-drive control and ODESC mower motor control
- [stm32_low_level_a](stm32_low_level_a/README.md): low-level drivetrain-oriented control
- [stm32_low_level_b](stm32_low_level_b/README.md): low-level mower, lift, turret, or auxiliary I/O control

Pin maps, protocols, and firmware build instructions will be added after the wiring plan stabilizes.
