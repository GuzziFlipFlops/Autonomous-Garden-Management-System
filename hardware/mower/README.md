# Mower Hardware

The front mower module is mechanically done.

## Motor

The mower uses a C6374-class BLDC motor with high peak-power capability. The system is 36V. Normal use is expected to be far lower than the peak capability of the motor/controller hardware.

## Controller

The current controller is a low-cost ODrive-style FOC controller, around $40, for torque-capable BLDC control. A simpler version could use a cheaper sensorless e-bike-style BLDC controller.

## Safety

The mower should be treated as dangerous during all tests. Confirm safe enable/disable behavior before mower-powered operation.
