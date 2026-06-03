# Mower Module

The front mower module is mechanically done. It uses a C6374-class BLDC motor with high peak-power capability.

## Motor and Controller

The robot is a 36V system. The mower motor and controller hardware have high burst capability, while normal mowing use is expected to be far lower than the hardware peak.

The current controller is a low-cost ODrive-style FOC controller, around $40, for torque-capable BLDC control. A simpler version could use a cheaper sensorless e-bike-style BLDC controller.

## Integration

The mower module is mounted in the front section of the robot. It works with the rope-based lift mechanism so the mower can be raised and lowered.

## Testing Needed

- Controller bring-up with the blade removed or the mower safely disabled
- Low-power spin tests
- Thermal checks
- Safe enable/disable behavior
- Mower lift interaction checks
- Outdoor cutting tests only after drivetrain and safety systems are reliable
