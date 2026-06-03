# Bill of Materials

This BOM is a working cost tracker for the Autonomous Garden Management System.

The current design target is a core platform cost around $250, but that is not a final cost claim. The final number depends on the battery, fasteners, wiring, sensors, spare parts, 3D-printing material, and any parts reused from previous builds.

| Subsystem | Part | Estimated Cost | Notes |
|---|---|---:|---|
| Frame | Aluminum channel, around 16 ft | TBD | Main chassis material, sourced from Lowe's |
| Drivetrain | DS3240 servos | TBD | Repurposed as low-cost drivetrain motors |
| High-level control | ESP32-P4 | TBD | Main autonomy and coordination controller |
| Low-level control | 2x STM32 F103 Blue Pill | TBD | Encoder reading, motor PWM, BTS7960 control, sensor I/O |
| Drivetrain drivers | BTS7960 modules | TBD | Low-cost motor driver modules |
| Mower motor | C6374-class BLDC motor | TBD | High peak-power capability, normal use expected far lower |
| Mower controller | ODrive-style FOC controller | Around $40 | Low-cost torque-capable BLDC control |
| IMU | BNO085 | TBD | Orientation and motion sensing |
| Turret base | Worm-drive rotating base | TBD | First hose turret degree of freedom |
| Turret elevation | Second hose turret axis | TBD | Planned hose up/down angle control |
| Lift system | Rope-based mower lift | TBD | Custom mower up/down mechanism |
| Power | 36V battery and converters | TBD | Main bus and regulated logic rails |
| Wiring | Wire, connectors, terminals, fuses | TBD | Must be rated for expected current |
| Protection | Signal isolation/protection parts | TBD | For noisy motor and low-voltage logic boundaries |
| Hardware | Bolts, nuts, brackets, standoffs | TBD | Final quantity not yet locked |

## Cost Philosophy

GMS is being designed around practical low-cost substitutions:

- Aluminum channel instead of custom machining
- ESP32-P4 instead of a Raspberry Pi 5-class SBC
- STM32 F103 boards for low-level real-time I/O
- DS3240 servos repurposed as drivetrain motors
- Commodity BTS7960 motor drivers
- Low-cost ODrive-style FOC controller for the mower motor
- DIY rope-based lift mechanism
- Printable and hand-built mechanical assemblies

## Still Needed

- Exact supplier links
- Exact quantities
- Actual paid prices
- Optional and minimum viable configurations
- Spare part allowance
- Weight estimates
- Battery cost decision
- Final controller and sensor choices
