# Safety

This is an experimental outdoor robotics project. It uses a 36V electrical system, high-current motor controllers, drive motors, rotating mower hardware, and liquid handling. Treat every subsystem as unproven until it has been tested safely.

## General Testing

- Keep people and pets away from the robot during powered tests.
- Use a controlled test area with clear space around the robot.
- Start at low speed and low power.
- Secure the robot during early motor tests so it cannot drive away unexpectedly.
- Keep a physical emergency stop or main disconnect within reach.
- Disconnect power before changing wiring or mechanical parts.
- Log failures and update the design before increasing power or speed.

## Mower Safety

- Never test the mower near people or pets.
- Test drivetrain and autonomy with the mower disabled first.
- Use physical guards where practical.
- Do not run the mower indoors.
- Keep hands, clothing, wires, and tools away from rotating components.
- Confirm safe enable/disable behavior before any mower-powered test.
- Do not rely only on software for mower safety.

## Electrical Safety

- The robot uses a 36V power system.
- High-current wiring should be fused close to the battery.
- Use wire gauges, connectors, and switches rated for expected current.
- Verify polarity before powering the system.
- Motor controllers can create electrical noise and voltage spikes.
- Use signal isolation/protection where practical between noisy motor electronics and low-voltage logic.
- Keep logic wiring physically separated from high-current motor wiring where possible.
- Add strain relief so motion and vibration do not pull wires loose.

## Battery Safety

- Use a battery chemistry and current rating appropriate for the drivetrain and mower loads.
- Add a main fuse or breaker close to the battery.
- Use a main disconnect switch.
- Do not leave batteries charging unattended.
- Do not operate damaged battery packs.
- Store and charge batteries according to the manufacturer's instructions.

## Liquid Application Safety

- Test liquid systems with water first.
- Do not use any controlled liquid application module until the pump, valve, hose, and software shutoff are reliable.
- Follow all local rules and product safety instructions for any future liquid use.
- Avoid unintended plants, drains, ponds, people, and pets.
- Add both software and hardware disable paths for pumps and valves.

## Outdoor Testing

- Start on flat ground before grass, slopes, or uneven terrain.
- Keep a safe standoff distance.
- Use a spotter when practical.
- Stop testing if wiring, motors, batteries, or controllers become hot.
- Inspect the mower module, lift, drivetrain, and turret after every test.
