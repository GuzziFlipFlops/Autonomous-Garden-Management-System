# Autonomous Garden Management System (GMS)

The **Autonomous Garden Management System**, or **GMS**, is a low-cost outdoor robotics platform for garden maintenance tasks such as mowing, hose positioning, and controlled liquid application.

The project is built around an aluminum-channel chassis, an ESP32-P4 high-level controller, STM32 F103 low-level microcontrollers, repurposed high-torque servo motors for drivetrain actuation, and modular garden-tooling hardware.

This repository is currently an early-stage build log and engineering documentation hub. Firmware, CAD files, wiring diagrams, and field-test data will be added as each subsystem becomes stable.

---

## Current Build Status

### Completed

- Main drivetrain mechanical system
- Front mower module
- High-power BLDC mower motor wiring
- Rope-based mower height lift mechanism
- Main front-frame structure
- Initial FOC motor controller wiring
- Initial worm-gear rotating turret base

### In Progress

- ESP32-P4 high-level control system
- STM32 F103 low-level motor and sensor control
- Encoder wiring and drivetrain feedback
- Rear turret module
- Hose aiming system
- Control box layout
- Full wiring documentation

### Planned

- 2-DOF water-hose turret
- Base-station docking with hose connection
- Electrically controlled water flow
- Modular controlled liquid-application system
- Outdoor navigation and obstacle sensing
- Field testing videos and performance logs

---

## Project Goals

GMS is designed around four main goals:

1. **Low cost**  
   The target is to keep the core robot platform around the $250 range by using inexpensive aluminum channel, low-cost microcontrollers, repurposed servo motors, and commodity motor controllers.

2. **Outdoor robustness**  
   The frame is built from aluminum channel instead of relying on a fragile 3D-printed-only structure. The chassis is designed to tolerate rough terrain, vibration, grass, and minor impacts.

3. **Modular garden tooling**  
   The front of the robot contains the mowing system. The rear is being developed as a modular turret system for watering and other garden tasks.

4. **Microcontroller-based autonomy**  
   Instead of using an expensive Raspberry Pi 5-class single-board computer, the robot is designed around an ESP32-P4 for high-level control and STM32 F103 boards for low-level real-time I/O.

---

## Mechanical Overview

The chassis uses approximately 16 feet of aluminum channel as the main frame material. This keeps the robot affordable while still providing a strong outdoor structure.

The aluminum frame also gives the robot a small amount of natural compliance. This may help the robot absorb uneven terrain compared with a completely rigid chassis.

The robot is divided into two main sections:

### Front section

The front section contains:

- Drivetrain
- Mower motor
- Mower height lift mechanism
- Main power electronics
- FOC motor controller wiring

This part of the robot is the most complete.

### Rear section

The rear section is still being developed. It will contain:

- Rotating turret base
- Hose aiming system
- Docking interface for a base-station hose
- Controlled liquid-application hardware

The first turret degree of freedom is a worm-gear rotating base. The second degree of freedom will raise and lower the hose angle.

---

## Mower System

The mower module uses a C6374-class BLDC motor. The motor hardware has high peak-power capability compared with typical low-cost mower motors.

The robot currently uses a 36V electrical system. Although the mower motor and controller hardware are capable of high burst power, normal operating power is expected to be much lower than the maximum hardware rating.

The current controller is a low-cost ODrive-style FOC controller. A simpler version of the project could use a cheaper sensorless e-bike-style BLDC controller.

---

## Mower Height Lift Mechanism

The mower height mechanism uses a custom rope-based lift system to raise and lower the mower assembly.

This allows the cutting height to be adjusted electronically instead of manually. The goal is to make the mower module more flexible during autonomous operation.

---

## Drivetrain

The drivetrain uses repurposed DS3240 servo motors as low-cost drive motors.

A 4-pack of these motors costs much less than most traditional robotics drivetrain motors, which helps keep the project affordable. The servos are being used as high-torque drivetrain actuators instead of standard position-control servos.

The drivetrain will eventually use encoder feedback for movement estimation and low-level closed-loop control.

---

## Electronics Architecture

GMS uses a distributed microcontroller architecture.

### High-level controller

- ESP32-P4
- Handles high-level autonomy logic
- Coordinates low-level controllers
- Manages robot behavior and garden-tool operation

### Low-level controllers

- 2x STM32 F103 Blue Pill microcontrollers
- Handle real-time I/O
- Read encoder signals
- Control motor PWM outputs
- Interface with motor drivers
- Reduce timing load on the ESP32-P4

### Sensors and I/O

Planned or current I/O includes:

- 4 analog encoder signals
- 8 motor PWM signals
- BTS7960 motor drivers
- BNO085 IMU
- Stepper motor control
- DS3240 servo control
- BLDC mower motor controller
- Turret motor control
- Hose/base-station interface

---

## Control Box

The control box is designed to protect the main electronics from outdoor electrical noise, motor surges, and wiring faults.

The system uses protection and isolation between low-voltage control electronics and noisy motor-power electronics wherever practical. The goal is to prevent drivetrain or mower power faults from damaging the high-level controller.

The control box will contain:

- ESP32-P4 high-level controller
- STM32 F103 low-level controllers
- Motor driver wiring
- Power distribution
- Signal isolation/protection
- Sensor wiring terminals
- Safety disconnects
- Debug/programming access

---

## Watering Turret

The rear turret system is being developed as a 2-degree-of-freedom hose aiming module.

The first degree of freedom is the rotating base. The second degree of freedom will raise and lower the hose angle.

The robot is intended to connect to a base station with a hose attachment. When the robot parks at the base station, it can attach to or detach from the hose system and aim water toward plants using the turret.

Planned features:

- 2-DOF hose aiming
- Base-station docking
- Electrically controlled water flow
- Plant-targeted watering
- Modular rear tooling

---

## Liquid Application Module

A small rear-mounted container and pump may be used for controlled liquid application.

This module is intended to be modular and should only be used with safe, legal, and properly handled liquids. Testing should be performed with water before any other liquid is used.

---

## Cost Philosophy

The project is designed to be affordable by avoiding expensive robotics parts wherever possible.

Examples:

- Aluminum channel frame instead of a custom machined frame
- ESP32-P4 instead of a Raspberry Pi 5-class single-board computer
- STM32 F103 boards for low-level I/O
- Repurposed DS3240 servos as drivetrain motors
- Low-cost BLDC controller options
- Commodity BTS7960 motor drivers
- 3D-printed and DIY mechanical assemblies

The target cost for the core platform is around $250. A full cost breakdown will be added in `BOM.md` as the design stabilizes.

---

## Media

Images will be added in the `media/` folder.

Planned media:

- Main robot photo
- CAD overview render
- Drivetrain photos
- Mower lift mechanism photos
- Turret base photos
- Wiring diagrams

---

## Repository Status

This repository is early-stage.

Current contents focus on:

- Mechanical design notes
- System architecture
- Build logs
- Wiring plans
- CAD organization
- Test documentation

Firmware will be added as each subsystem becomes stable.

---

## Safety Notice

This is an experimental outdoor robotics project.

The mower system uses high-power rotating hardware and must be treated as dangerous. Testing should be done with proper guards, emergency shutoff, controlled power, and no people or pets near the robot.

Liquid application systems should be tested with water first and must follow all local rules and product safety instructions.

---

## License

License to be selected.
