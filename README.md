# GTricks Pathfinder

**Smart Posts for Remote Hiking Trails**  
Industrial-Grade AIoT Safety Network for Zero-Signal Environments

---

> *Safety is never out of reach, even when signals are.*

---

## Overview

PathFinder is an industrial-grade AIoT safety network engineered for remote environments where conventional telecommunications infrastructure is entirely absent. The system deploys autonomous smart posts at 2–5 km intervals along hiking trails, coastal zones, and wilderness areas, creating a self-sustaining, offline-first mesh network that enables emergency communication, real-time environmental monitoring, and wildlife threat detection — without any dependency on cellular networks, internet connectivity, or satellite subscriptions.

The project was developed by Team GTricks at the University of Moratuwa for the SLIoT Challenge 2026 under the theme *AIoT for a Sustainable Future*.

---

## Table of Contents

- [Problem Statement](#problem-statement)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
  - [Pathfinder Node](#pathfinder-node)
  - [Base Station](#base-station)
  - [PCB Design](#pcb-design)
- [Software](#software)
  - [Node Firmware](#node-firmware)
  - [Base Station Firmware](#base-station-firmware)
  - [Python Bridge](#python-bridge)
  - [Command Center Dashboard](#command-center-dashboard)
  - [WiFi Captive Portal](#wifi-captive-portal)
- [AI and TinyML](#ai-and-tinyml)
- [Communication Protocol](#communication-protocol)
- [Signal Performance](#signal-performance)
- [Installation and Setup](#installation-and-setup)
- [Project Structure](#project-structure)
- [Market Applications](#market-applications)
- [Business Model](#business-model)
- [Future Development](#future-development)
- [Contributors](#contributors)
- [License](#license)

---

## Problem Statement

Remote hiking trails, coastal fishing zones, and wilderness areas across Sri Lanka share a critical infrastructure gap: the complete absence of emergency communication capability beyond conventional cellular coverage. Survey data collected from 38 respondents confirmed that 97.4% of hikers have experienced sudden loss of 4G/5G signal in the wilderness, with 65.8% describing it as a major safety concern.

The consequences of this gap are measurable. Sri Lanka's rescue services conduct 180–240 hiker search-and-rescue operations annually in the central highlands at an average cost of LKR 450,000–1,200,000 per operation, totalling LKR 100–200 million in annual government expenditure. Human-elephant conflict along forest boundaries causes 100+ human deaths and 250+ elephant deaths per year, with total economic damage exceeding LKR 3 billion annually.

Existing solutions — satellite messengers, EPIRB beacons, satellite phones — cost USD 250–2,500 per device with recurring subscription fees of USD 12–50 per month, making them economically inaccessible for the majority of Sri Lankan hikers and coastal fishermen. PathFinder addresses this gap at a manufacturing cost of LKR 5,700–6,500 per node with no recurring operational costs.

---

## System Architecture

The system operates as a three-tier architecture:

```
[ PathFinder Node ]
  ESP32 + RA-02 LoRa
  BME280 / MPU6050 / INMP441
  WiFi Captive Portal
  SOS Button + RGB LED
        |
        | LoRa 433 MHz (2–3 km per hop)
        |
[ Arduino Uno Base Station ]
  RA-02 LoRa Receiver
  JSON Serial Output
        |
        | USB Serial (115200 baud)
        |
[ Python Bridge — base_station.py ]
  Flask + Flask-SocketIO
  Serial listener thread
  Heartbeat parser
  Auto COM port detection
        |
        | WebSocket (Socket.IO)
        |
[ Command Center Dashboard — dashboard.html ]
  Three.js 3D terrain
  Live event log
  RSSI sparkline
  Alert state management
```

**Data flow:**
1. The PathFinder Node collects environmental and seismic data, serves the WiFi captive portal to hikers, and transmits LoRa alert and heartbeat packets.
2. The Arduino Uno base station receives LoRa packets and forwards them as structured JSON over USB serial.
3. The Python bridge reads the serial stream, parses packet types, and pushes data to all connected browser clients via Socket.IO.
4. The browser dashboard renders a live 3D command center with real-time updates from the field node.

---

## Hardware

### Pathfinder Node

The node is the field-deployed unit mounted on a post at the trail location. It runs entirely on local power (prototype: USB; production: solar + LiPo) and operates without any internet connection.

| Component | Specification | Role |
|-----------|--------------|------|
| ESP32 WROOM-32 | Xtensa dual-core 240 MHz, 520 KB SRAM, WiFi + BT | Main MCU, WiFi access point, sensor hub |
| RA-02 LoRa Module | SX1276, 433 MHz, −148 dBm sensitivity, 18 dBm TX | Long-range radio communication |
| BME280 | Temp ±0.5°C, Humidity ±3%, Pressure ±1 hPa | Environmental sensing |
| MPU6050 | 6-axis IMU, ±16 g accelerometer, 1 kHz output | Seismic vibration detection |
| INMP441 | I2S MEMS microphone, −26 dBFS, SNR 61 dB | Acoustic wildlife detection |
| RGB LED | Common cathode, 5 mm diffused | Visual status indicator |
| SOS Push Button | Momentary N/O, IP67-rated, 22 mm yellow cap | Physical emergency trigger |
| Custom PCB | 0.6 mm tracks, 1.0 mm safety clearance, 2-layer FR4 | Structural integration |
| 433 MHz Antenna | SMA stubby, 3 dBi, IP65 | RF transmission |

**PCB Pin Mapping (from schematic):**

| Signal | GPIO |
|--------|------|
| SOS Button | GPIO 15 |
| LED Red | GPIO 13 |
| LED Green | GPIO 12 |
| LED Blue | GPIO 27 |
| LoRa NSS/SS | GPIO 5 |
| LoRa RESET | GPIO 14 |
| LoRa DIO0 | GPIO 2 |
| LoRa SCK | GPIO 18 |
| LoRa MISO | GPIO 19 |
| LoRa MOSI | GPIO 23 |
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |

**LED Status Codes:**

| Colour | State |
|--------|-------|
| Solid Green | System secure, normal operation |
| Solid Red (5 s) | SOS alert transmitted |
| Solid Purple (5 s) | Seismic diagnostic alert transmitted |
| White (boot) | Initialisation in progress |

### Base Station

The base station is deployed at the ranger post, base camp, or monitoring centre. It receives all LoRa transmissions from field nodes and forwards them to a connected laptop running the Python bridge.

| Component | Specification |
|-----------|--------------|
| Arduino Uno | ATmega328P, 5V logic |
| RA-02 LoRa Module | SX1276, 433 MHz |
| USB cable | To laptop running base_station.py |

**Arduino Uno SPI Wiring:**

| LoRa Signal | Arduino Uno Pin |
|------------|----------------|
| NSS/CS | Digital 10 |
| RESET | Digital 9 |
| DIO0 | Digital 2 |
| MOSI | Digital 11 |
| MISO | Digital 12 |
| SCK | Digital 13 |
| VCC | 3.3V |
| GND | GND |

**Important:** The RA-02 module operates at 3.3V logic. A logic level shifter on MOSI, SCK, and NSS lines is recommended to protect the module from the Uno's 5V outputs.

### PCB Design

The custom PCB was designed for environmental durability in field conditions:

- 0.6 mm copper track width for signal integrity
- 1.0 mm safety clearance between power and signal traces
- 2-layer FR4 substrate
- SMA bulkhead connector with silicone O-ring for antenna sealing
- Designed to be mounted inside a 3D-printed PLA+ enclosure (prototype) or IP65-rated ABS injection-moulded enclosure (production)

**Enclosure (Prototype):**
- 3D-printed PLA+, approximately 120 × 100 × 60 mm
- Tamper-resistant Torx M3 mounting screws
- QR code label for WiFi portal access
- GTricks Pathfinder branding
- Approximate material cost: LKR 1,000

---

## Software

### Node Firmware

**File:** `node/node.ino`  
**Platform:** Arduino IDE / PlatformIO with ESP32 board support

The node firmware manages five concurrent responsibilities in a cooperative loop:

1. **WiFi Captive Portal** — Serves a full offline web interface on `192.168.4.1` via DNS redirect. Any device connecting to the `Pathfinder-Net` access point is automatically redirected to the portal, requiring no URL entry.

2. **Hardware SOS Button** — Polls GPIO 15 with a 2-second debounce cooldown. On press, transmits a `HARDWARE_SOS_TRIGGERED` LoRa alert packet and sets the LED red for 5 seconds.

3. **Heartbeat Transmission** — Every 30 seconds, reads all sensors and transmits a heartbeat packet containing node ID, temperature, humidity, and pressure. This keeps the base station dashboard live without requiring any user interaction.

4. **Sensor Reading** — BME280 is the primary sensor. If BME280 is not detected, the firmware falls back to MPU6050 for temperature. Sensor detection is attempted at both I2C addresses (0x76 and 0x77) automatically.

5. **LoRa Transmission** — All packets are transmitted using LoRa SF7, BW125 kHz, CR4/5 at 18 dBm. These parameters must match the base station configuration exactly.

**LoRa packet formats:**

```
Alert packet:
"PF-KNUCKLES-01, ALERT: HARDWARE_SOS_TRIGGERED"
"PF-KNUCKLES-01, ALERT: VIRTUAL_WEB_SOS"
"PF-KNUCKLES-01, ALERT: AI_SEISMIC_TARGET_DETECTED"

Heartbeat packet:
"PF-KNUCKLES-01, HEARTBEAT, TEMP:28.4, HUM:71, PRES:1013.2"
```

**Required Arduino libraries:**
- `LoRa` by Sandeep Mistry
- `Adafruit BME280 Library`
- `Adafruit MPU6050`
- `Adafruit Unified Sensor`

### Base Station Firmware

**File:** `base/base.ino`  
**Platform:** Arduino IDE with standard AVR support

Receives LoRa packets and outputs structured JSON over USB serial at 115200 baud. JSON escaping is applied to the payload string to prevent malformed output if special characters are present.

**Output format:**
```json
{"type": "alert", "payload": "PF-KNUCKLES-01, ALERT: HARDWARE_SOS_TRIGGERED", "rssi": -56, "snr": 9.5}
{"status": "Base Station Active. Listening 433MHz SF7 BW125..."}
{"error": "LoRa init failed. Check wiring & 3.3V power."}
```

**Important:** Close the Arduino IDE Serial Monitor before running `base_station.py`. Windows COM ports are exclusive — only one application can hold the port open at a time.

### Python Bridge

**File:** `base_station.py`  
**Requirements:** `pyserial`, `flask`, `flask-socketio`

```bash
pip install pyserial flask flask-socketio
```

The bridge performs three roles simultaneously:

- **Serial listener thread** — Opens the Arduino COM port, reads JSON lines, and emits them to all connected Socket.IO clients. Uses `errors='ignore'` on decode to prevent garbled LoRa bytes from crashing the thread. Wraps the serial connection in an outer retry loop to reconnect automatically if the Arduino is unplugged.

- **Heartbeat parser** — Detects packets containing `HEARTBEAT` in the payload and parses the `TEMP:`, `HUM:`, and `PRES:` fields into separate dictionary keys before forwarding to the dashboard.

- **Flask web server** — Serves `templates/dashboard.html` at `http://localhost:5000`.

**Auto COM port detection** — The script scans connected serial ports for USB descriptors matching CH340, FTDI, CP210, or Arduino keywords and selects the correct port automatically. Manual override is available by setting `ARDUINO_PORT` at the top of the file.

**Running:**
```bash
python base_station.py
```

Expected output on successful startup:
```
[INFO] Available serial ports:
        COM8      USB Serial Device (COM8)
[INFO] Using Arduino on: COM8
[READY] Dashboard: http://localhost:5000
[OK] Serial port COM8 opened at 115200 baud
[RX] {'status': 'Base Station Active. Listening 433MHz SF7 BW125...'}
```

**Folder structure required:**
```
your-project-folder/
├── base_station.py
└── templates/
    └── dashboard.html
```

### Command Center Dashboard

**File:** `templates/dashboard.html`  
**Dependencies:** Three.js r128 (CDN with 3-CDN fallback), Socket.IO 4.0.1 (CDN)

A standalone HTML file that renders a military-grade 3D command center interface in the browser. No build step or package manager is required.

**Features:**

- **3D Terrain** — Procedural mountain terrain generated with Three.js using multi-octave sine noise. Vertex colours produce a warm olive-to-amber ramp from valley to peak. Drag to orbit, scroll to zoom. The terrain represents the Knuckles Mountain Range (6°47'22"N, 80°44'05"E, 1247m elevation).

- **Live Beacon** — A 3D beacon pole and sphere marks the PathFinder node position on the terrain. The beacon pulses teal (normal) or red (alert) in real time. Ripple rings expand from the beacon base during alert states.

- **Socket.IO Handler** — The Socket.IO client is initialised independently of the Three.js scene, outside the `init3D()` function, to ensure packets received during page load are never dropped. A retry loop polls for Socket.IO availability every 300ms before connecting.

- **Event Log** — All incoming packets are logged with timestamps. SOS events are shown in red, heartbeats in teal, general alerts in amber.

- **RSSI Sparkline** — A live canvas-rendered waveform tracks signal strength over time for the last 42 packets.

- **Simulation Buttons** — Three buttons allow demo simulation of SOS, seismic alert, and heartbeat packets without requiring live hardware.

**Alert state behaviour:**

When an SOS or seismic packet arrives:
- Status badge changes to red with the alert type
- Red border pulses around the entire viewport
- Beacon sphere and point light turn red
- Ground disc opacity increases
- Ripple rings switch from teal to red
- Alert auto-clears after 9 seconds

**CDN fallback chain for Three.js:**
1. `unpkg.com` (primary)
2. `cdn.jsdelivr.net` (fallback 1)
3. `cdnjs.cloudflare.com` (fallback 2)

If all three fail, an error banner is displayed.

### WiFi Captive Portal

The captive portal is served by the ESP32 directly from flash memory using a `PROGMEM` string. It is accessible offline, requires no internet, and works on Android, iOS, and desktop browsers.

**Features:**
- Trail map (SVG topographic overview with animated active position marker)
- Live sensor data (temperature, humidity, pressure — polled every 2 seconds from `/sensordata`)
- Safety status indicator (SECURE / SEISMIC ALERT)
- Automated trail guide chatbot (offline keyword-based response system)
- Virtual SOS button (triggers LoRa transmission via `/trigger_sos`)
- Seismic diagnostic button (triggers test alert via `/simulate_alert`)

**Captive portal endpoints:**

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main portal interface |
| `/sensordata` | GET | JSON sensor readings |
| `/trigger_sos` | GET | Transmit virtual SOS |
| `/simulate_alert` | GET | Transmit seismic diagnostic |
| `/generate_204` | GET | Android captive portal redirect |
| `/hotspot-detect.html` | GET | iOS captive portal redirect |

**Accessing the portal:**  
Scan the QR code on the node enclosure using any smartphone camera. No app installation required. The QR code encodes the WiFi credentials for `Pathfinder-Net` (open network, no password) in the standard WiFi QR format. The phone connects and the browser opens automatically via captive portal detection.

---

## AI and TinyML

PathFinder integrates two parallel on-device intelligence pipelines trained using Edge Impulse and deployed as TensorFlow Lite Micro inference libraries on the ESP32.

### Audio Analysis Pipeline

- **Sensor:** INMP441 MEMS I2S microphone
- **Target:** Elephant vocalisations (95–115 Hz fundamental with 14–24 Hz ground coupling), heavy animal footfall acoustic signature
- **Feature extraction:** MFE (Mel-filterbank energy) spectrograms
- **Model:** MLP neural network, optimised to fit within ESP32 SRAM constraints
- **Dataset:** 2,400 audio samples — ambient forest recordings mixed with synthesised elephant infrasound at −6 dB, 0 dB, and +6 dB SNR levels
- **Accuracy:** ~72% in simulated noisy field conditions

**Challenge:** The forest background noise floor (wind, rain, insects, birds) measures −38 to −42 dBFS, substantially masking low-frequency infrasound at the INMP441's sensitivity range. No labelled Sri Lankan wildlife audio dataset existed; a custom dataset was generated using published frequency-domain data from WWF Sri Lanka.

### Vibration Analysis Pipeline

- **Sensor:** MPU6050 6-axis IMU
- **Target:** Heavy animal movement (>500 kg), landslide precursor vibrations, ground tremor
- **Sampling rate:** 100 Hz
- **Model:** Threshold classifier with spectral features
- **Dataset:** 1,800 vibration samples recorded by dropping known weights (50 kg, 200 kg, 500 kg equivalent) on soil substrate
- **Accuracy:** ~85% on custom dataset

### Dual-Sensor Confirmation

Both pipelines run independently. An alert is only transmitted if both audio AND vibration triggers fire within a 3-second window. This dual-confirmation approach reduces the combined false positive rate from ~28% (single sensor) to below 4% (dual sensor), making the system operationally viable for ranger alert workflows where false positives carry real operational cost.

---

## Communication Protocol

### LoRa Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 433.0 MHz |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| TX Power | 18 dBm |
| Preamble | 8 symbols (default) |

Both the node and base station must use identical LoRa parameters. The parameters are set explicitly in both firmware files to prevent silent mismatch from library defaults.

**Regulatory status:** LoRa at 433 MHz falls under the Sri Lanka Telecommunications Regulatory Commission's Short Range Devices licence-exempt category (TRC Circular 2019/04) at power levels below 25 dBm ERP. PathFinder operates at 18 dBm — no radio licence is required per installation.

### JSON Schema (Base Station Serial Output)

**Alert packet:**
```json
{
  "type": "alert",
  "payload": "PF-KNUCKLES-01, ALERT: HARDWARE_SOS_TRIGGERED",
  "rssi": -56,
  "snr": 9.5
}
```

**Heartbeat packet (as emitted to dashboard after Python parsing):**
```json
{
  "type": "alert",
  "payload": "PF-KNUCKLES-01, HEARTBEAT, TEMP:28.4, HUM:71, PRES:1013.2",
  "rssi": -74,
  "snr": 8.2,
  "temp": "28.4",
  "hum": "71",
  "pres": "1013.2"
}
```

**Status packet:**
```json
{
  "status": "Base Station Active. Listening 433MHz SF7 BW125..."
}
```

---

## Signal Performance

All tests were conducted on the University of Moratuwa campus — a semi-urban environment with multi-storey concrete buildings, metal roofing, dense vegetation, and significant RF interference from WiFi access points and mobile towers. These conditions represent a worst-case scenario and are substantially more obstructive than open mountain or forest terrain.

| Distance | RSSI (dBm) | SNR (dB) | Packet Loss | Status |
|----------|-----------|---------|-------------|--------|
| 100 m | −52 to −58 | +9.0 to +10.5 | 0% | Excellent |
| 200 m | −63 to −70 | +6.5 to +8.2 | 0% | Good |
| 500 m | −75 to −82 | +3.0 to +5.5 | 5–8% | Acceptable |
| 700 m | −88 to −96 | −1.0 to +1.8 | 25–35% | Marginal |
| >700 m | <−100 | <−3.0 | >60% | Link Failure |

**Field projection:** The RA-02 SX1276 module is rated for 5–10 km line-of-sight under ideal conditions. Campus test results are degraded by building wall loss (15–25 dB per wall), WiFi interference (+5 dB noise floor), and GSM/LTE interference. None of these factors are present in the Knuckles Mountain Range target deployment environment. Projected reliable range in field conditions: **2–3 km per hop at SF7**, **4–6 km per hop at SF12**.

---

## Installation and Setup

### Prerequisites

**Software:**
- Arduino IDE 2.x with ESP32 board support (Espressif Systems 2.x)
- Python 3.9 or later
- pip packages: `pyserial`, `flask`, `flask-socketio`

**Arduino libraries (install via Library Manager):**
- `LoRa` by Sandeep Mistry
- `Adafruit BME280 Library`
- `Adafruit MPU6050`
- `Adafruit Unified Sensor`

### Step 1 — Flash the PathFinder Node

1. Open `node/node.ino` in Arduino IDE
2. Select board: `ESP32 Dev Module`
3. Verify pin definitions at the top of the file match your PCB:
   ```cpp
   #define SOS_PIN    15
   #define LED_R      13
   #define LED_G      12
   #define LED_B      27
   #define LORA_SS     5
   #define LORA_RST   14
   #define LORA_DIO0   2
   #define I2C_SDA    21
   #define I2C_SCL    22
   ```
4. Upload. Open Serial Monitor at 115200 baud and verify:
   ```
   [OK] LoRa radio 433MHz SF7 BW125 CR4/5
   [OK] WiFi AP 'Pathfinder-Net' active
   [READY] All systems nominal
   ```

### Step 2 — Flash the Base Station

1. Open `base/base.ino` in Arduino IDE
2. Select board: `Arduino Uno`
3. Upload
4. Open Serial Monitor at 115200 baud and verify:
   ```
   {"status": "Base Station Active. Listening 433MHz SF7 BW125..."}
   ```
5. Close the Serial Monitor (important — Python cannot access the port while Arduino IDE holds it open)

### Step 3 — Run the Python Bridge

```bash
pip install pyserial flask flask-socketio
python base_station.py
```

Expected output:
```
[INFO] Available serial ports:
        COM8      USB Serial Device (COM8)
[INFO] Using Arduino on: COM8
[READY] Dashboard: http://localhost:5000
[OK] Serial port COM8 opened at 115200 baud
```

If auto-detection fails, set the port manually at the top of `base_station.py`:
```python
ARDUINO_PORT = 'COM8'       # Windows
ARDUINO_PORT = '/dev/ttyUSB0'  # Linux/macOS
```

### Step 4 — Open the Dashboard

Open `http://localhost:5000` in Google Chrome. The 3D terrain should render and the event log should show `SOCKET_CONNECTED`.

### Step 5 — Test the System

Press the physical SOS button on the node enclosure. Within 2–3 seconds the dashboard should show:
- Status badge changing to `SOS ACTIVE`
- Red border animation around the viewport
- Beacon turning red in the 3D scene
- Event log entry with RSSI and SNR values

### WiFi Portal Test

Connect a phone to the `Pathfinder-Net` WiFi network (no password). The browser should open automatically showing the trail portal. Press `Transmit Emergency SOS` — the Python terminal should show a new `[RX]` packet.

---

## Project Structure

```
GTricks-Pathfinder/
├── node/
│   └── node.ino                  # ESP32 node firmware
├── base/
│   └── base.ino                  # Arduino Uno base station firmware
├── base_station.py               # Python Flask + Socket.IO bridge
├── templates/
│   └── dashboard.html            # 3D command center dashboard
├── docs/
│   ├── pathfinder_pitch_report.docx   # Full pitch and strategy document
│   ├── pathfinder_dashboard.html      # Statistics infographic dashboard
│   ├── GTrickds.pdf                   # Business roadmap poster
│   └── PathFinder_The_Smart_Post.pdf  # Product brochure
├── pcb/
│   └── pcb.jpeg                  # PCB schematic
└── README.md
```

---

## Market Applications

PathFinder is designed as a horizontal platform deployable across five distinct sectors:

**Hiking Trails and Eco-Tourism**  
Sri Lanka's national parks and wilderness trails receive 800,000+ visitors annually with zero emergency communication infrastructure beyond 2 km from trailheads. PathFinder nodes provide offline trail maps, real-time weather alerts, and SOS capability. A single helicopter rescue costs LKR 800,000–1,200,000; PathFinder amortises its full deployment cost with the prevention of a single incident.

**Coastal Fishing Industry**  
Sri Lanka has 250,000+ registered fishing craft, of which 82% are artisanal vessels operating beyond cellular range without emergency communication equipment. PathFinder nodes on adjacent vessels form a floating mesh network, relaying GPS coordinates and SOS signals to shore base stations without satellite infrastructure. Cost comparison: EPIRB beacon LKR 45,000–120,000 with annual subscription versus PathFinder at LKR 8,000 one-time.

**Plantations**  
Sri Lanka's 1.1 million hectares of tea, rubber, and coconut plantation employ 300,000+ workers in areas without cellular coverage. PathFinder provides worker safety communication, automated check-in monitoring, and environmental data collection for irrigation scheduling and climate modelling.

**Industrial Facilities**  
Large factories (garment, cement, power) experience communication dead zones inside metal structures where WiFi attenuates by 30–40 dB. LoRa at 433 MHz provides 8–12 dB better wall penetration than 2.4 GHz WiFi at equivalent distances. PathFinder fulfils Sri Lanka Factories Ordinance emergency communication requirements at 60% lower cost than wired intercom alternatives.

**Maritime Bulk Carriers**  
Cargo vessels experience communication blackouts in metal cargo holds and engine rooms. PathFinder internal mesh nodes relay crew location and man-overboard alerts to the bridge at zero recurring cost, compared to Iridium satellite handsets at USD 800–2,500 per unit with USD 1.50/minute airtime rates.

---

## Business Model

### Unit Economics

| Item | Cost (LKR) |
|------|-----------|
| Electronics BOM | 4,450 |
| 3D-printed enclosure | 1,250 |
| Total manufacturing cost | ~5,700 |
| Selling price (starter) | 12,000 |
| Gross margin | ~110% |

### Package Tiers

| Package | Coverage | Price (LKR) | Gross Profit (LKR) |
|---------|----------|-------------|-------------------|
| Starter | 1–5 km | 35,000 | ~15,000 |
| Standard | 5–15 km | 75,000 | ~35,000 |
| Professional | 15–30 km | 165,000 | ~80,000 |
| Enterprise | 30 km+ | 320,000+ | ~160,000+ |

### 3-Year Projection

| Metric | Year 1 | Year 2 | Year 3 |
|--------|--------|--------|--------|
| Installations | 8 | 35 | 120 |
| Revenue (LKR) | 360,000 | 2,625,000 | 11,400,000 |
| Gross Profit (LKR) | 185,000 | 1,575,000 | 7,200,000 |
| Maintenance Revenue | 0 | 160,000 | 700,000 |

### Industry Validation

> "Pathfinder is an innovative device designed to provide essential support for adventure travelers exploring remote locations where telecommunications signals are unavailable. It ensures reliable assistance and enhances safety in environments beyond conventional network coverage."  
> — Damitha EashwaraArachchi, Manager, Centuria Wild Resort, Udawalawe

---

## Future Development

### Phase 2 — Mesh Topology

The current prototype implements a star topology where each node transmits directly to the base station. Phase 2 transitions to a full LoRa mesh network using the RadioLib LoraMesh implementation, enabling self-healing routes, multi-hop packet relay, and coverage scaling without linear infrastructure cost increases.

A 25 km trail covered by 12 mesh nodes costs LKR 96,000 to deploy. The equivalent cellular infrastructure (one tower) costs LKR 8–15 million — a 99.4% cost reduction.

### Phase 2 — Hardware Upgrades

| Upgrade | Specification | Purpose |
|---------|--------------|---------|
| IP65 ABS enclosure | Injection-moulded, EPDM gasket | Weatherproofing |
| Conformal coating | MG Chemicals 419C acrylic | PCB moisture protection |
| Solar power | 6W panel + 3× 18650 LiPo | Self-sustaining operation |
| Battery backup | 33.3 Wh, 73 hours autonomy | 3-day cloudy weather tolerance |
| Yagi antenna (base) | 6–9 dBi directional | Extended range |
| SF12 mode | 4–6× range, lower data rate | Deep wilderness coverage |

### Power Budget (Production Target)

| Mode | Current Draw |
|------|-------------|
| Active WiFi TX | 240 mA |
| ESP32 deep sleep | 10 µA |
| Average daily consumption | ~45 mAh/day |
| Solar input (5.2 peak hours) | 6W panel sufficient |
| Battery autonomy (no solar) | ~73 hours |

### Anti-Theft Measures

- Tamper detection via MPU6050 — abnormal vibration transmits `TAMPER_ALERT` before power is cut
- Tamper-resistant Torx T20H security screws (not available in retail hardware stores)
- Node transmits last known position on boot; geographic displacement flags `RELOCATED_NODE` alert at base

---

## Contributors

| Name | Role |
|------|------|
| S.V.C. Hansaja Vithanage | Technology Lead |
| Senuja Dilmith Geeganage | Entrepreneurship Lead |
| T.A.S. Dulsika Mendis | Sustainability Lead |
| M.K. Madhuka Gihan Chinthaka | Team Member |
| B.A. Vindya Nilushika | Team Member |

**Institution:** University of Moratuwa, Sri Lanka  
**Competition:** SLIoT Challenge 2026 — AIoT for a Sustainable Future  
**Team Name:** GTricks

---

## License

This repository is released for academic and evaluation purposes in the context of the SLIoT Challenge 2026. Commercial use, reproduction, or redistribution of the PCB design, firmware, or AI model architecture without explicit written permission from Team GTricks is not permitted.

The dual-sensor fusion algorithm for wildlife detection (seismic + acoustic co-confirmation) is the subject of a provisional patent application with the National Intellectual Property Office of Sri Lanka (NIPO).

For licensing inquiries, contact the team through the University of Moratuwa.

---

*Team GTricks — University of Moratuwa — SLIoT Challenge 2026*
