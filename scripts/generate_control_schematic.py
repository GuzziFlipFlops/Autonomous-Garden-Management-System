from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


OUTPUT = Path("media/control_architecture_ascii.png")

SCHEMATIC = r"""
+----------------------------------------------------------------------------------+
|                     AUTONOMOUS GARDEN ROVER CONTROL SCHEMATIC                    |
+----------------------------------------------------------------------------------+

                         +-----------------------------+
                         | HIGH LEVEL CONTROLLER       |
                         | ESP32-P4                    |
                         | autonomy / coordination     |
                         +--------------+--------------+
                                        |
                 +----------------------+----------------------+
                 |                      |                      |
              UART                   UART                   UART
                 |                      |                      |
                 v                      v                      v
      +---------------------+  +---------------------+  +---------------------+
      | STM32 F103 A        |  | STM32 F103 B        |  | ODRIVE PRO          |
      | drivetrain I/O      |  | tool / actuator I/O |  | mower BLDC control  |
      +----------+----------+  +----------+----------+  +----------+----------+
                 |                        |                        |
                 |                        |                        |
        motor control lines        step / dir / en            BLDC phases
                 |                    and aux motor              + feedback
                 v                        v                        v

      +---------------------+      +---------------------+      +--------------+
      | Motor Controller 1  |      | Stepper Driver 1    |      | Mower Motor  |
      | drive motor / wheel |      | STEP DIR EN         |      | C6374-class  |
      +---------------------+      +---------------------+      +--------------+

      +---------------------+      +---------------------+
      | Motor Controller 2  |      | Stepper Driver 2    |
      | drive motor / wheel |      | STEP DIR EN         |
      +---------------------+      +---------------------+

      +---------------------+      +---------------------+
      | Motor Controller 3  |      | Stepper Driver 3    |
      | drive motor / wheel |      | STEP DIR EN         |
      +---------------------+      +---------------------+

      +---------------------+      +---------------------+
      | Motor Controller 4  |      | Other Motor Wire 1  |
      | drive motor / wheel |      | Other Motor Wire 2  |
      +---------------------+      | Other Motor Wire 3  |
                                   +---------------------+

Notes:
- ESP32-P4 talks to both STM32 F103 boards over UART.
- ESP32-P4 also talks to the ODrive Pro over UART.
- STM32 F103 A maps to four drivetrain motor controllers.
- STM32 F103 B maps to three stepper drivers, with three wires each: STEP, DIR, EN.
- STM32 F103 B also keeps three extra motor-control wires for other motors.
""".strip("\n")


def load_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
    ]
    for candidate in candidates:
        path = Path(candidate)
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def main() -> None:
    font = load_font(26)
    lines = SCHEMATIC.splitlines()

    dummy = Image.new("RGB", (1, 1))
    draw = ImageDraw.Draw(dummy)
    bboxes = [draw.textbbox((0, 0), line, font=font) for line in lines]
    line_widths = [bbox[2] - bbox[0] for bbox in bboxes]
    line_heights = [bbox[3] - bbox[1] for bbox in bboxes]

    padding = 48
    line_spacing = 10
    width = max(line_widths) + padding * 2
    height = sum(line_heights) + line_spacing * (len(lines) - 1) + padding * 2

    image = Image.new("RGB", (width, height), "#f8faf7")
    draw = ImageDraw.Draw(image)

    y = padding
    for line, line_height in zip(lines, line_heights):
        draw.text((padding, y), line, font=font, fill="#17201a")
        y += line_height + line_spacing

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT)
    print(f"Wrote {OUTPUT} ({width}x{height})")


if __name__ == "__main__":
    main()
