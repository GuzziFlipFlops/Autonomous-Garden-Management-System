# ESP32 Tank Drive + ODESC Control

This folder contains the ESP32 PlatformIO/ESP-IDF firmware for the tank-drive web GUI.

## Stored Copies

- `src/esp32_odesc_idf_main.cpp`: active firmware with drivetrain controls, ODESC speed controls, emergency stop, and footer telemetry.
- `working_baseline/`: known-good drivetrain + write-only ODESC control version saved before adding telemetry reads.
- `legacy_micropython/main.py`: older MicroPython/Thonny prototype kept for reference.

## Wiring

### BTS7960 Left Side

- PWM forward: ESP32 GPIO14
- PWM reverse: ESP32 GPIO12
- Current sense: ESP32 GPIO34 and GPIO35
- Enable pins: tied to 3.3 V

### BTS7960 Right Side

- PWM forward: ESP32 GPIO26
- PWM reverse: ESP32 GPIO27
- Current sense: ESP32 GPIO32 and GPIO33
- Enable pins: tied to 3.3 V

### ODESC UART

- ESP32 GPIO16 TX -> ODESC GPIO2 RX
- ESP32 GPIO17 RX <- ODESC GPIO1 TX
- ESP32 GND -> ODESC GND
- UART baud: 115200

## Web GUI

- ESP32 AP SSID: `ODESC-MOTOR`
- URL from a connected phone/laptop: `http://192.168.4.1`

The top button is an emergency stop. When emergency stop is active, the ESP32 stops the drivetrain and repeatedly sends ODESC stop commands until outputs are enabled again.

The drivetrain section has both a joystick and the original button pad. The joystick uses differential mixing, so it can drive forward/backward and turn at the same time. The button pad remains available for direct forward/back/left/right commands.

The ODESC section uses velocity-ramp input mode instead of passthrough. This keeps speed changes from stepping instantly to the target, which reduces current spikes during sensorless startup. The GUI exposes:

- current limit
- max speed
- brake resistor resistance in ohms
- velocity ramp in turn/s^2

If no brake resistor is connected, set brake resistance to `0`. If a brake resistor is connected, set this to the actual resistor value.

## Footer Telemetry

The footer shows:

- system/bus voltage from `vbus_voltage`
- ODrive total current from `ibus`
- ODrive input power as `vbus_voltage * ibus`
- estimated BTS7960 current for the left and right controllers
- ODESC FET temperature from `axis0/axis1.fet_thermistor.temperature`, with `.temp` as a fallback

ODESC reads run in a low-priority background task and are cached before the GUI requests status, so button requests do not wait on UART telemetry replies.

The BTS7960 current values are estimates from the IS pins. The code auto-zeros the ADC offsets while the drivetrain is stopped. Treat these values as rough telemetry unless the exact BTS7960 board current-sense resistor and scaling are verified.

## Build

From this folder:

```bash
pio run -e esp32_odesc_idf
```

Flash with PlatformIO if the ESP32 is on `/dev/ttyUSB0`:

```bash
pio run -e esp32_odesc_idf -t upload
```

If high-speed upload is flaky, use the built binaries with `esptool.py` at 115200 baud.
