import serial
import serial.tools.list_ports
import json
import threading
import time
from flask import Flask, render_template
from flask_socketio import SocketIO

# --- Configuration ---
ARDUINO_PORT = 'COM8'   # <--- CHANGE to your port (e.g. '/dev/ttyUSB0' on Linux/Mac)
BAUD_RATE    = 115200
RETRY_DELAY  = 3        # seconds between reconnect attempts

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

def listen_to_radio():
    """
    Background thread: opens the serial port, reads JSON lines from the
    Arduino, and emits them over WebSocket to the dashboard.

    FIX 1: Wraps serial.Serial() in an outer while-True loop so the thread
            automatically reconnects if the Arduino is unplugged and replugged.

    FIX 2: Uses errors='ignore' on decode() so garbled LoRa bytes never
            raise UnicodeDecodeError and crash the thread permanently.
    """
    while True:
        try:
            ser = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)
            print(f"[OK] Connected to Arduino on {ARDUINO_PORT}")
            socketio.emit('new_lora_packet', {'status': 'BASE_STATION_RECONNECTED'})

            while True:
                # FIX 2: errors='ignore' drops bad bytes instead of crashing
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                if not line:
                    continue

                try:
                    data = json.loads(line)
                    print(f"[RX] {data}")

                    # Route heartbeat packets so dashboard can update sensors
                    payload = data.get('payload', '')
                    if 'HEARTBEAT' in payload:
                        # Parse "PF-KNUCKLES-01, HEARTBEAT, TEMP:28.4, HUM:71"
                        parts = {p.split(':')[0].strip(): p.split(':')[1].strip()
                                 for p in payload.split(',')
                                 if ':' in p}
                        data['temp'] = parts.get('TEMP', None)
                        data['hum']  = parts.get('HUM',  None)

                    socketio.emit('new_lora_packet', data)

                except json.JSONDecodeError:
                    print(f"[RAW] Non-JSON line: {line}")

        except serial.SerialException as e:
            print(f"[ERROR] Serial port error: {e}")
            print(f"  Retrying in {RETRY_DELAY}s... (is the Arduino plugged in?)")
            try:
                ser.close()
            except Exception:
                pass
            time.sleep(RETRY_DELAY)

        except Exception as e:
            print(f"[ERROR] Unexpected error: {e}")
            time.sleep(RETRY_DELAY)


@app.route('/')
def index():
    return render_template('dashboard.html')


if __name__ == '__main__':
    # List available COM ports to help with setup
    ports = [p.device for p in serial.tools.list_ports.comports()]
    print(f"[INFO] Available serial ports: {ports}")
    if ARDUINO_PORT not in ports:
        print(f"[WARN] '{ARDUINO_PORT}' not found. Edit ARDUINO_PORT at the top of this file.")

    thread = threading.Thread(target=listen_to_radio)
    thread.daemon = True
    thread.start()

    print("Command Center running — open http://localhost:5000 in Chrome.")
    socketio.run(app, host='0.0.0.0', port=5000, allow_unsafe_werkzeug=True)
