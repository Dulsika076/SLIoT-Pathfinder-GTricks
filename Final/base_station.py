"""
═══════════════════════════════════════════════════════════════════════════
  PATHFINDER BASE STATION BRIDGE
  Arduino Uno USB Serial  ->  Flask + Socket.IO  ->  Dashboard (browser)
═══════════════════════════════════════════════════════════════════════════

  Features:
    - Auto-detects Arduino COM port (CH340 / FTDI / USB-Serial)
    - Heartbeat parsing:  TEMP / HUM / PRES fields broken out for dashboard
    - Robust decoding:    errors='ignore' so garbled LoRa bytes don't crash
    - Auto-reconnect:     thread retries every 3 s if Arduino is unplugged
    - Serves dashboard:   http://localhost:5000

  Requirements:
    pip install pyserial flask flask-socketio
"""

import serial
import serial.tools.list_ports
import json
import threading
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

# ═══════════════════════ CONFIG ═══════════════════════
ARDUINO_PORT   = None         # None = auto-detect; or set to 'COM8' / '/dev/ttyUSB0'
BAUD_RATE      = 115200
RETRY_DELAY    = 3            # seconds between reconnect attempts
HOST           = '0.0.0.0'
PORT           = 5000

# ═══════════════════════ FLASK ═══════════════════════
app      = Flask(__name__)
socketio = SocketIO(
    app,
    cors_allowed_origins="*",
    async_mode='threading',     # must match threaded serial listener
    logger=False,
    engineio_logger=False,
    ping_timeout=60,            # wait 60s before declaring client dead
    ping_interval=25            # heartbeat every 25s
)


def find_arduino_port():
    """Auto-detect the Arduino's COM port by USB descriptor."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None

    # Prefer common Arduino USB-to-serial chips
    keywords = ['CH340', 'CH9102', 'Arduino', 'USB Serial', 'FTDI', 'CP210', 'Silicon Labs']
    for p in ports:
        desc = (p.description or '') + ' ' + (p.manufacturer or '')
        if any(k.lower() in desc.lower() for k in keywords):
            return p.device

    # Fallback — return the first port if only one present
    if len(ports) == 1:
        return ports[0].device
    return None


# ═══════════════════════ HEARTBEAT PARSER ═══════════════════════
def parse_heartbeat(payload: str) -> dict:
    """
    Parse a heartbeat payload like:
        "PF-KNUCKLES-01, HEARTBEAT, TEMP:28.4, HUM:71, PRES:1013.2"
    Returns a dict with temp/hum/pres keys (strings) where present.
    """
    out = {}
    for segment in payload.split(','):
        segment = segment.strip()
        if ':' not in segment:
            continue
        key, value = segment.split(':', 1)
        key = key.strip().upper()
        value = value.strip()
        if key == 'TEMP': out['temp'] = value
        elif key == 'HUM':  out['hum']  = value
        elif key == 'PRES': out['pres'] = value
    return out


# ═══════════════════════ SERIAL LISTENER THREAD ═══════════════════════
def listen_to_radio(port: str):
    """
    Background thread: opens serial, reads JSON lines from Arduino, emits
    them over WebSocket. Reconnects automatically if the port disappears.
    """
    while True:
        ser = None
        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=1)
            print(f"[OK] Serial port {port} opened at {BAUD_RATE} baud")
            socketio.emit('new_lora_packet', {'status': 'BASE_STATION_RECONNECTED'})

            while True:
                raw = ser.readline().decode('utf-8', errors='ignore').strip()
                if not raw:
                    continue

                try:
                    data = json.loads(raw)
                except json.JSONDecodeError:
                    print(f"[RAW] Non-JSON line: {raw}")
                    continue

                print(f"[RX] {data}")

                # If it's an alert with payload, check for heartbeat and enrich
                payload = data.get('payload', '') or ''
                if 'HEARTBEAT' in payload:
                    parsed = parse_heartbeat(payload)
                    data.update(parsed)

                socketio.emit('new_lora_packet', data)

        except serial.SerialException as e:
            print(f"[ERROR] Serial error: {e}")
            print(f"        Retrying in {RETRY_DELAY}s (is the Arduino plugged in?)")

        except Exception as e:
            print(f"[ERROR] Unexpected: {e}")

        finally:
            if ser is not None:
                try: ser.close()
                except Exception: pass

        time.sleep(RETRY_DELAY)


# ═══════════════════════ FLASK ROUTES ═══════════════════════
@app.route('/')
def index():
    return render_template('dashboard.html')


# ═══════════════════════ MAIN ═══════════════════════
if __name__ == '__main__':
    # Show detected ports
    ports = list(serial.tools.list_ports.comports())
    print("\n╔════════════════════════════════════════════╗")
    print("║    PATHFINDER BASE STATION — STARTING      ║")
    print("╚════════════════════════════════════════════╝\n")

    print("[INFO] Available serial ports:")
    for p in ports:
        print(f"        {p.device:8s}  {p.description}")

    # Resolve port
    port = ARDUINO_PORT or find_arduino_port()
    if port is None:
        print("\n[WARN] No Arduino auto-detected!")
        print("       Set ARDUINO_PORT manually at the top of this file.\n")
        port = 'COM8'   # fallback so thread can still retry
    else:
        print(f"\n[INFO] Using Arduino on: {port}\n")

    # Start listener thread
    thread = threading.Thread(target=listen_to_radio, args=(port,), daemon=True)
    thread.start()

    # Start Flask server
    print(f"[READY] Dashboard: http://localhost:{PORT}")
    print(f"[READY] Remote:    http://<your-ip>:{PORT}\n")
    socketio.run(app, host=HOST, port=PORT, allow_unsafe_werkzeug=True)