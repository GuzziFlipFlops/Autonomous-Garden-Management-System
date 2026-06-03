# Safety

This project is experimental and uses high-power motors, outdoor drivetrain hardware, liquid systems, and rotating mower components.

## Mower Safety

- Never test the mower near people or pets.
- Use an emergency stop.
- Test drivetrain and autonomy with the mower disabled first.
- Use physical guards where possible.
- Do not run the mower indoors.
- Keep hands away from all rotating components.
- Disconnect power before modifying wiring.
- Secure the robot during early motor tests so it cannot drive away unexpectedly.

## Electrical Safety

- The robot uses a 36V power system.
- High-current wiring should be fused.
- Motor controllers can generate electrical noise and voltage spikes.
- Signal electronics should be isolated or protected where practical.
- Verify common grounds, isolation boundaries, and power polarity before powering the system.
- Use wire gauges and connectors rated for the expected current.
- Keep logic wiring physically separated from high-current motor wiring where possible.

## Battery Safety

- Use a battery chemistry and pack current rating appropriate for the mower and drivetrain loads.
- Add a main fuse or breaker close to the battery.
- Use a main disconnect switch.
- Do not leave batteries charging unattended.
- Do not operate damaged battery packs.

## Liquid Application Safety

- Test liquid systems with water first.
- Do not spray chemicals autonomously without legal, environmental, and safety review.
- Follow all label instructions for any garden chemical.
- Avoid spraying near people, pets, drains, ponds, or unintended plants.
- Add a software and hardware disable for any pump or valve.

## Outdoor Testing

- Start with low-speed drivetrain tests.
- Test on flat ground before grass or slopes.
- Log failures and update the design before increasing speed or power.
- Keep a safe distance from the robot during autonomous tests.
