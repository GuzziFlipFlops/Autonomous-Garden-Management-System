# Bill of Materials

The project is designed around a low-cost target of approximately **$250 for the core platform**.

Final cost depends on battery choice, motor controller choice, sensor set, reused parts, fasteners, wiring, and 3D-printing material.

| Subsystem | Part | Estimated Cost | Notes |
|---|---:|---:|---|
| Frame | Aluminum channel, ~16 ft | TBD | Main chassis material |
| Drivetrain | DS3240 servo motors | TBD | Repurposed as drive motors |
| High-level control | ESP32-P4 | ~$20 | Main autonomy controller |
| Low-level control | STM32 F103 Blue Pill x2 | TBD | Real-time motor/sensor I/O |
| Motor drivers | BTS7960 modules | TBD | Drivetrain control |
| Mower motor | C6374-class BLDC motor | TBD | High-power mower motor |
| Mower controller | ODrive-style FOC controller | ~$40 | Low-cost BLDC FOC controller |
| IMU | BNO085 | TBD | Orientation and motion sensing |
| Turret | Worm-gear base | TBD | First turret degree of freedom |
| Lift system | Rope-based mechanism | TBD | Mower height control |
| Power | Battery and converters | TBD | 36V system |
| Wiring | Connectors, wire, terminals, protection | TBD | Must be sized for expected current |
| Hardware | Bolts, nuts, standoffs, brackets | TBD | Final count TBD |

## Cost Notes

The cost target should be treated as a design constraint, not a finished claim, until the BOM is complete.

Major cost-saving decisions:

- Aluminum-channel frame instead of custom machining
- ESP32-P4 instead of a Raspberry Pi 5-class board
- STM32 F103 boards for low-level control
- Repurposed DS3240 servos as drivetrain motors
- Commodity motor drivers
- Low-cost FOC controller option for the mower motor
- DIY rope-based lift mechanism
- 3D-printed and hand-built mechanical assemblies

## To Add

- Exact supplier links
- Exact quantities
- Actual paid prices
- Weight of each subsystem
- Optional upgrade parts
- Minimum viable build configuration
