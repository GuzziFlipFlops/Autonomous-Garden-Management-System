import gc
import network
import socket
import time
from machine import Pin, UART


# Thonny/MicroPython file for an ESP32 DevKit controlling an ODESC/ODrive
# v3.x compatible board over UART ASCII.
#
# Wiring used here:
#   ODrive GPIO1 / UART TX -> ESP32 GPIO18 / RX
#   ODrive GPIO2 / UART RX <- ESP32 GPIO19 / TX
#   ODrive GND             -> ESP32 GND

AP_SSID = "ODESC-MOTOR"
UART_ID = 2
UART_BAUD = 115200
UART_RX_PIN = 18
UART_TX_PIN = 19
UART_ALT_RX_PIN = 19
UART_ALT_TX_PIN = 18

AXIS = 0
MOTOR = 0

AXIS_STATE_IDLE = 1
AXIS_STATE_SENSORLESS_CONTROL = 5
CONTROL_MODE_VELOCITY = 2
INPUT_MODE_PASSTHROUGH = 1

DEFAULT_SPEED_TURNS_PER_SEC = 5.0
DEFAULT_MAX_SPEED_TURNS_PER_SEC = 15.0
DEFAULT_CURRENT_LIMIT_A = 10.0
DEFAULT_DURATION_SEC = 0.0

SENSORLESS_RAMP_CURRENT_A = 4.0
SENSORLESS_RAMP_VEL_ERAD_S = 219.91
SENSORLESS_RAMP_ACCEL_ERAD_S2 = 80.0
SENSORLESS_RAMP_TIME_S = 0.8

ODRIVE_WRITE_DELAY_MS = 8
ODRIVE_READ_TIMEOUT_MS = 500
KEEPALIVE_MS = 250
POLL_MS = 1200
UART_DRAIN_CHUNK = 32
UART_DRAIN_MAX_BYTES = 512


uart = None
uart_rx_pin = UART_RX_PIN
uart_tx_pin = UART_TX_PIN


state = {
    "target_speed": DEFAULT_SPEED_TURNS_PER_SEC,
    "direction": 1,
    "signed_speed": DEFAULT_SPEED_TURNS_PER_SEC,
    "max_speed": DEFAULT_MAX_SPEED_TURNS_PER_SEC,
    "current_limit": DEFAULT_CURRENT_LIMIT_A,
    "duration": DEFAULT_DURATION_SEC,
    "running": False,
    "run_until": 0,
    "last_keepalive": 0,
    "last_poll": 0,
    "last_action": "boot",
    "last_error": "",
    "vbus": "",
    "axis_state": "",
    "axis_error": "",
    "motor_error": "",
    "controller_error": "",
    "sensorless_error": "",
    "input_vel": "",
    "uart_rx_pin": UART_RX_PIN,
    "uart_tx_pin": UART_TX_PIN,
    "uart_ok": False,
}

events = []


def init_uart(rx_pin=UART_RX_PIN, tx_pin=UART_TX_PIN):
    global uart, uart_rx_pin, uart_tx_pin
    uart_rx_pin = rx_pin
    uart_tx_pin = tx_pin
    state["uart_rx_pin"] = rx_pin
    state["uart_tx_pin"] = tx_pin
    uart = UART(
        UART_ID,
        baudrate=UART_BAUD,
        tx=Pin(tx_pin),
        rx=Pin(rx_pin),
        bits=8,
        parity=None,
        stop=1,
        timeout=80,
    )
    log("UART{} baud={} rx=GPIO{} tx=GPIO{}".format(UART_ID, UART_BAUD, rx_pin, tx_pin))
    return uart


def ticks_ms():
    return time.ticks_ms()


def ticks_add(a, b):
    return time.ticks_add(a, b)


def ticks_diff(a, b):
    return time.ticks_diff(a, b)


def log(message):
    print("[odesc-ap] {}".format(message))
    events.append(message)
    if len(events) > 40:
        events.pop(0)


def clamp(value, low, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def parse_float(value, default):
    try:
        return float(value)
    except Exception:
        return default


def parse_int(value, default):
    try:
        return int(float(value))
    except Exception:
        return default


def json_escape(value):
    return str(value).replace("\\", "\\\\").replace('"', '\\"').replace("\n", " ")


def drain_uart():
    if uart is None:
        return
    drained = 0
    while uart.any():
        waiting = uart.any()
        size = UART_DRAIN_CHUNK
        if waiting and waiting < size:
            size = waiting
        data = uart.read(size)
        if not data:
            break
        drained += len(data)
        if drained >= UART_DRAIN_MAX_BYTES:
            log("UART drain stopped after {} bytes; more data may remain".format(drained))
            break
    if drained:
        log("UART drained {} stale bytes".format(drained))


def odrive_write(command):
    if uart is None:
        state["last_error"] = "UART not initialized"
        return
    line = command.strip()
    log("TX {}".format(line))
    uart.write((line + "\n").encode())
    time.sleep_ms(ODRIVE_WRITE_DELAY_MS)


def odrive_readline(timeout_ms=ODRIVE_READ_TIMEOUT_MS):
    if uart is None:
        state["last_error"] = "UART not initialized"
        return ""
    deadline = ticks_add(ticks_ms(), timeout_ms)
    buf = b""
    while ticks_diff(deadline, ticks_ms()) > 0:
        if uart.any():
            chunk = uart.read(1)
            if not chunk:
                continue
            if chunk == b"\n":
                text = buf.decode("utf-8", "ignore").strip()
                log("RX {}".format(text))
                return text
            if chunk != b"\r":
                if len(buf) < 96:
                    buf += chunk
                else:
                    log("RX line too long; dropping buffer")
                    buf = b""
        time.sleep_ms(2)
    return ""


def odrive_query(property_name, timeout_ms=ODRIVE_READ_TIMEOUT_MS):
    drain_uart()
    odrive_write("r {}".format(property_name))
    reply = odrive_readline(timeout_ms)
    if not reply:
        state["last_error"] = "No UART reply for {}".format(property_name)
    return reply


def uart_waiting():
    if uart is None:
        return -1
    try:
        return uart.any()
    except Exception as exc:
        log("UART any() failed: {}".format(exc))
        return -1


def test_uart_link():
    gc.collect()
    log("UART test begin: RX GPIO{} TX GPIO{} free={} any={}".format(
        uart_rx_pin, uart_tx_pin, gc.mem_free(), uart_waiting()
    ))
    reply = odrive_query("vbus_voltage", timeout_ms=900)
    log("UART test end: reply='{}' free={} any={}".format(
        reply, gc.mem_free(), uart_waiting()
    ))
    ok = bool(reply)
    state["uart_ok"] = ok
    if ok:
        state["vbus"] = reply
        state["last_error"] = ""
        state["last_action"] = "UART OK: vbus {} V".format(reply)
    else:
        state["last_action"] = "UART test failed"
        state["last_error"] = "No UART reply. Check common GND, TX/RX crossover, and whether this ESP32 pin is usable."
    return ok


def swap_uart_pins():
    if uart_rx_pin == UART_RX_PIN and uart_tx_pin == UART_TX_PIN:
        init_uart(UART_ALT_RX_PIN, UART_ALT_TX_PIN)
    else:
        init_uart(UART_RX_PIN, UART_TX_PIN)
    state["uart_ok"] = False
    state["last_action"] = "UART pins changed to RX GPIO{} / TX GPIO{}".format(uart_rx_pin, uart_tx_pin)
    test_uart_link()


def odrive_write_axis(path, value):
    odrive_write("w axis{}.{} {}".format(AXIS, path, value))


def clear_errors():
    odrive_write_axis("error", 0)
    odrive_write_axis("motor.error", 0)
    odrive_write_axis("controller.error", 0)
    odrive_write_axis("sensorless_estimator.error", 0)
    state["last_error"] = ""


def configure_runtime(max_speed, current_limit):
    current_limit = clamp(current_limit, 1.0, 60.0)
    max_speed = clamp(max_speed, 0.5, 80.0)

    odrive_write("w config.enable_uart 1")
    odrive_write("w config.uart_baudrate {}".format(UART_BAUD))
    odrive_write("w config.brake_resistance 0")
    odrive_write_axis("config.enable_step_dir", 0)
    odrive_write_axis("config.enable_watchdog", 0)
    odrive_write_axis("motor.config.current_lim", current_limit)
    odrive_write_axis("motor.config.direction", 1)
    odrive_write_axis("controller.config.control_mode", CONTROL_MODE_VELOCITY)
    odrive_write_axis("controller.config.input_mode", INPUT_MODE_PASSTHROUGH)
    odrive_write_axis("controller.config.vel_limit", max_speed)
    odrive_write_axis("config.sensorless_ramp.current", SENSORLESS_RAMP_CURRENT_A)
    odrive_write_axis("config.sensorless_ramp.vel", SENSORLESS_RAMP_VEL_ERAD_S)
    odrive_write_axis("config.sensorless_ramp.accel", SENSORLESS_RAMP_ACCEL_ERAD_S2)
    odrive_write_axis("config.sensorless_ramp.ramp_time", SENSORLESS_RAMP_TIME_S)
    odrive_write_axis("config.sensorless_ramp.finish_on_vel", 1)
    odrive_write_axis("config.sensorless_ramp.finish_on_distance", 0)
    odrive_write_axis("config.sensorless_ramp.finish_on_enc_idx", 0)


def stop_motor():
    odrive_write_axis("requested_state", AXIS_STATE_IDLE)
    odrive_write_axis("controller.input_vel", 0)
    odrive_write("v {} 0 0".format(MOTOR))
    state["running"] = False
    state["run_until"] = 0
    state["signed_speed"] = 0.0
    state["last_action"] = "stopped"
    log("Motor stop requested")


def start_motor(speed, direction, duration, current_limit, max_speed):
    direction = 1 if direction >= 0 else -1
    speed = clamp(abs(speed), 0.0, max_speed)
    signed_speed = speed * direction

    state["target_speed"] = speed
    state["direction"] = direction
    state["signed_speed"] = signed_speed
    state["duration"] = max(0.0, duration)
    state["current_limit"] = current_limit
    state["max_speed"] = max_speed

    if not test_uart_link():
        state["running"] = False
        state["signed_speed"] = 0.0
        return

    clear_errors()
    configure_runtime(max_speed, current_limit)

    # Sensorless startup needs the requested velocity present before entering
    # sensorless control, otherwise the board can lock in at zero and fault.
    odrive_write_axis("controller.input_vel", signed_speed)
    odrive_write_axis("requested_state", AXIS_STATE_SENSORLESS_CONTROL)
    odrive_write("v {} {} 0".format(MOTOR, signed_speed))

    state["running"] = True
    state["last_keepalive"] = ticks_ms()
    state["run_until"] = 0
    if duration > 0.0:
        state["run_until"] = ticks_add(ticks_ms(), int(duration * 1000))
    state["last_action"] = "running at {:.2f} turn/s".format(signed_speed)
    log(state["last_action"])


def apply_speed(speed, direction):
    if not state["uart_ok"] and not test_uart_link():
        state["running"] = False
        state["signed_speed"] = 0.0
        return

    direction = 1 if direction >= 0 else -1
    speed = clamp(abs(speed), 0.0, state["max_speed"])
    signed_speed = speed * direction
    state["target_speed"] = speed
    state["direction"] = direction
    state["signed_speed"] = signed_speed
    odrive_write_axis("controller.input_vel", signed_speed)
    odrive_write("v {} {} 0".format(MOTOR, signed_speed))
    state["last_action"] = "speed set to {:.2f} turn/s".format(signed_speed)


def poll_odrive(force=False):
    now = ticks_ms()
    if not force and ticks_diff(now, state["last_poll"]) < POLL_MS:
        return
    state["last_poll"] = now

    state["vbus"] = odrive_query("vbus_voltage")
    state["axis_state"] = odrive_query("axis{}.current_state".format(AXIS))
    state["axis_error"] = odrive_query("axis{}.error".format(AXIS))
    state["motor_error"] = odrive_query("axis{}.motor.error".format(AXIS))
    state["controller_error"] = odrive_query("axis{}.controller.error".format(AXIS))
    state["sensorless_error"] = odrive_query("axis{}.sensorless_estimator.error".format(AXIS))
    state["input_vel"] = odrive_query("axis{}.controller.input_vel".format(AXIS))


def service_motor():
    now = ticks_ms()

    if state["running"] and state["run_until"]:
        if ticks_diff(now, state["run_until"]) >= 0:
            stop_motor()
            return

    if state["running"] and ticks_diff(now, state["last_keepalive"]) >= KEEPALIVE_MS:
        state["last_keepalive"] = now
        odrive_write_axis("controller.input_vel", state["signed_speed"])
        odrive_write("u {}".format(MOTOR))


def parse_query(path):
    data = {}
    if "?" not in path:
        return data
    query = path.split("?", 1)[1].split(" ", 1)[0]
    for part in query.split("&"):
        if not part:
            continue
        if "=" in part:
            key, value = part.split("=", 1)
        else:
            key, value = part, ""
        data[key] = value.replace("+", " ")
    return data


def status_json():
    items = [
        '"running":{}'.format("true" if state["running"] else "false"),
        '"speed":{}'.format(state["target_speed"]),
        '"direction":{}'.format(state["direction"]),
        '"signed_speed":{}'.format(state["signed_speed"]),
        '"max_speed":{}'.format(state["max_speed"]),
        '"current_limit":{}'.format(state["current_limit"]),
        '"duration":{}'.format(state["duration"]),
        '"vbus":"{}"'.format(json_escape(state["vbus"])),
        '"axis_state":"{}"'.format(json_escape(state["axis_state"])),
        '"axis_error":"{}"'.format(json_escape(state["axis_error"])),
        '"motor_error":"{}"'.format(json_escape(state["motor_error"])),
        '"controller_error":"{}"'.format(json_escape(state["controller_error"])),
        '"sensorless_error":"{}"'.format(json_escape(state["sensorless_error"])),
        '"input_vel":"{}"'.format(json_escape(state["input_vel"])),
        '"uart_ok":{}'.format("true" if state["uart_ok"] else "false"),
        '"uart_rx_pin":{}'.format(state["uart_rx_pin"]),
        '"uart_tx_pin":{}'.format(state["uart_tx_pin"]),
        '"last_action":"{}"'.format(json_escape(state["last_action"])),
        '"last_error":"{}"'.format(json_escape(state["last_error"])),
    ]
    return "{" + ",".join(items) + "}"


def logs_text():
    return "\n".join(events[-30:])


def send_response(client, body, content_type="text/plain; charset=utf-8", status="200 OK"):
    client.send("HTTP/1.1 {}\r\n".format(status))
    client.send("Content-Type: {}\r\n".format(content_type))
    client.send("Cache-Control: no-store\r\n")
    client.send("Connection: close\r\n\r\n")
    client.send(body)


def handle_api(path):
    query = parse_query(path)

    if path.startswith("/api/start"):
        max_speed = parse_float(query.get("max", state["max_speed"]), state["max_speed"])
        current = parse_float(query.get("current", state["current_limit"]), state["current_limit"])
        speed = parse_float(query.get("speed", state["target_speed"]), state["target_speed"])
        direction = parse_int(query.get("dir", state["direction"]), state["direction"])
        duration = parse_float(query.get("duration", state["duration"]), state["duration"])
        start_motor(speed, direction, duration, current, max_speed)
        return status_json()

    if path.startswith("/api/set_speed"):
        speed = parse_float(query.get("speed", state["target_speed"]), state["target_speed"])
        direction = parse_int(query.get("dir", state["direction"]), state["direction"])
        apply_speed(speed, direction)
        return status_json()

    if path.startswith("/api/stop"):
        stop_motor()
        return status_json()

    if path.startswith("/api/clear"):
        clear_errors()
        state["last_action"] = "errors cleared"
        return status_json()

    if path.startswith("/api/uart_test"):
        test_uart_link()
        return status_json()

    if path.startswith("/api/swap_uart"):
        swap_uart_pins()
        return status_json()

    if path.startswith("/api/poll"):
        poll_odrive(force=True)
        return status_json()

    if path.startswith("/api/status"):
        return status_json()

    if path.startswith("/api/logs"):
        return logs_text()

    return '{"error":"bad api"}'


def html_page():
    return """<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ODESC Motor</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:#f4f7f8;color:#152028}
header{background:#152028;color:white;padding:14px 16px}
h1{font-size:20px;margin:0}
main{max-width:620px;margin:0 auto;padding:14px}
section{margin:0 0 16px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label{display:grid;gap:5px;font-size:13px;color:#44535c}
input,select{width:100%;font-size:18px;padding:10px;border:1px solid #b8c3ca;border-radius:6px;background:white}
button{width:100%;min-height:54px;border:0;border-radius:6px;font-size:17px;font-weight:bold;background:#1e6f5c;color:white}
button.secondary{background:#324651}
button.stop{background:#b42318}
button.warn{background:#a15c00}
.speedline{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px}
.status{background:white;border:1px solid #d7e0e5;border-radius:6px;padding:10px;font-family:monospace;font-size:13px;white-space:pre-wrap;user-select:text}
.hint{font-size:13px;color:#566772;line-height:1.35}
.top{display:flex;justify-content:space-between;gap:12px;align-items:center}
.pill{font-family:monospace;background:#dfe8eb;border-radius:999px;padding:5px 9px;color:#152028}
@media(max-width:520px){.grid,.speedline{grid-template-columns:1fr}}
</style>
</head>
<body>
<header><div class="top"><h1>ODESC Motor</h1><div class="pill">AP 192.168.4.1</div></div></header>
<main>
<section class="grid">
<label>Speed, turn/s
<input id="speed" type="number" min="0" max="80" step="0.1" value="5.0">
</label>
<label>Direction
<select id="dir"><option value="1">Forward</option><option value="-1">Reverse</option></select>
</label>
<label>Run Time, seconds
<input id="duration" type="number" min="0" max="3600" step="0.1" value="0">
</label>
<label>Current Limit, A
<input id="current" type="number" min="1" max="60" step="0.1" value="10.0">
</label>
<label>Max Speed, turn/s
<input id="max" type="number" min="0.5" max="80" step="0.1" value="15.0">
</label>
</section>

<section class="speedline">
<button onclick="startMotor()">Start</button>
<button class="secondary" onclick="setSpeed()">Apply Speed</button>
<button class="stop" onclick="stopMotor()">Stop</button>
</section>

<section class="speedline">
<button class="warn" onclick="bump(-1)">-1 turn/s</button>
<button class="warn" onclick="bump(1)">+1 turn/s</button>
<button class="secondary" onclick="clearErrors()">Clear Errors</button>
</section>

<section class="speedline">
<button class="secondary" onclick="uartTest()">UART Test</button>
<button class="secondary" onclick="swapUart()">Try Swapped Pins</button>
<button class="secondary" onclick="refresh()">Refresh</button>
</section>

<section>
<div class="hint">Open Wi-Fi network: ODESC-MOTOR. Use 0 seconds for continuous run. Axis0 only. Normal wiring: ODrive GPIO1 TX to ESP32 GPIO18 RX, ODrive GPIO2 RX to ESP32 GPIO19 TX. If UART Test fails, check shared GND first, then try the swapped pin button.</div>
</section>

<section><div id="status" class="status">loading...</div></section>
<section><div id="logs" class="status"></div></section>
</main>

<script>
function val(id){return document.getElementById(id).value}
function setStatus(s){
  document.getElementById("status").textContent =
    "running: " + s.running + "\\n" +
    "uart_ok: " + s.uart_ok + "\\n" +
    "esp32_uart: RX GPIO" + s.uart_rx_pin + " / TX GPIO" + s.uart_tx_pin + "\\n" +
    "target: " + s.signed_speed + " turn/s\\n" +
    "input_vel: " + s.input_vel + "\\n" +
    "vbus: " + s.vbus + " V\\n" +
    "axis_state: " + s.axis_state + "\\n" +
    "axis_error: " + s.axis_error + "\\n" +
    "motor_error: " + s.motor_error + "\\n" +
    "controller_error: " + s.controller_error + "\\n" +
    "sensorless_error: " + s.sensorless_error + "\\n" +
    "last: " + s.last_action + "\\n" +
    "error: " + s.last_error;
}
function api(path){
  return fetch(path).then(function(r){return r.json()}).then(setStatus).catch(function(e){
    document.getElementById("status").textContent = "request failed: " + e;
  });
}
function startMotor(){
  api("/api/start?speed="+val("speed")+"&dir="+val("dir")+"&duration="+val("duration")+"&current="+val("current")+"&max="+val("max"));
}
function setSpeed(){
  api("/api/set_speed?speed="+val("speed")+"&dir="+val("dir"));
}
function stopMotor(){ api("/api/stop"); }
function clearErrors(){ api("/api/clear"); }
function uartTest(){ api("/api/uart_test"); }
function swapUart(){ api("/api/swap_uart"); }
function bump(delta){
  var el = document.getElementById("speed");
  var next = Math.max(0, Number(el.value || 0) + delta);
  el.value = next.toFixed(1);
  setSpeed();
}
function refresh(){
  api("/api/poll");
  fetch("/api/logs").then(function(r){return r.text()}).then(function(t){
    document.getElementById("logs").textContent = t;
  }).catch(function(){});
}
setInterval(refresh, 1800);
refresh();
</script>
</body>
</html>"""


def start_ap():
    ap = network.WLAN(network.AP_IF)
    ap.active(True)
    ap.config(essid=AP_SSID, authmode=getattr(network, "AUTH_OPEN", 0))
    while not ap.active():
        time.sleep_ms(100)
    return ap


def serve():
    ap = start_ap()
    log("AP active SSID={} IP={}".format(AP_SSID, ap.ifconfig()[0]))
    init_uart(UART_RX_PIN, UART_TX_PIN)
    test_uart_link()

    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(socket.getaddrinfo("0.0.0.0", 80)[0][-1])
    server.listen(4)
    server.settimeout(0.05)

    gc.collect()

    while True:
        service_motor()
        try:
            client, _ = server.accept()
        except OSError:
            continue

        try:
            request = client.recv(1024).decode("utf-8", "ignore")
            first_line = request.split("\r\n", 1)[0]
            path = first_line.split(" ")[1] if " " in first_line else "/"

            if path.startswith("/api/status") or path.startswith("/api/poll"):
                send_response(client, handle_api(path), "application/json")
            elif path.startswith("/api/"):
                body = handle_api(path)
                content_type = "application/json" if body.startswith("{") else "text/plain; charset=utf-8"
                send_response(client, body, content_type)
            else:
                send_response(client, html_page(), "text/html; charset=utf-8")
        except Exception as exc:
            send_response(client, "ERR {}".format(exc), "text/plain; charset=utf-8", "500 Internal Server Error")
        finally:
            client.close()
            gc.collect()


serve()
