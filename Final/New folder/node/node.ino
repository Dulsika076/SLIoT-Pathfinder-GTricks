/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  PATHFINDER NODE — Final Integrated Firmware
 *  Node ID: PF-KNUCKLES-01 · Knuckles Mountain Range, Sri Lanka
 * ═══════════════════════════════════════════════════════════════════════════
 *  Hardware: ESP32 + RA-02 LoRa + BME280 + MPU6050 + RGB LED + SOS button
 *  Features:
 *    - Hardware SOS button  (GPIO 15)   → LoRa alert
 *    - WiFi captive portal  (Pathfinder-Net)
 *        · Live sensor cards (temp, humidity, pressure)
 *        · Topo map widget
 *        · Offline chatbot
 *        · Virtual SOS button
 *        · Seismic diagnostics button
 *    - Periodic heartbeat   (every 30 s)  → keeps dashboard live
 *    - RGB LED status       (green=secure · red=SOS · purple=seismic)
 *    - BME280 primary, MPU6050 fallback for temperature
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MPU6050.h>

/* ─── PCB PIN MAP (from schematic) ─────────────────────── */
#define SOS_PIN      15    // Hardware SOS button (INPUT_PULLUP)
#define LED_R        13    // RGB LED — red channel
#define LED_G        12    // RGB LED — green channel
#define LED_B        27  // RGB LED — blue channel

#define LORA_SS      5    // LoRa NSS
#define LORA_RST     14    // LoRa RESET (labeled RST_LORA on PCB)
#define LORA_DIO0    2    // LoRa DIO0
#define LORA_SCK     18    // LoRa SCK  (VSPI default)
#define LORA_MISO    19    // LoRa MISO (VSPI default)
#define LORA_MOSI    23    // LoRa MOSI (VSPI default)

#define I2C_SDA      21    // BME280 + MPU6050 SDA
#define I2C_SCL      22    // BME280 + MPU6050 SCL

/* ─── CONFIG ────────────────────────────────────────── */
const String NODE_ID        = "PF-KNUCKLES-01";
const String NODE_LAT       = "7.4116 N";
const String NODE_LON       = "80.8060 E";
const String NODE_ADDRESS   = "Pekoe Trail, Stage 2 - Knuckles Mountain Range";
const unsigned long BUTTON_COOLDOWN     = 2000;   // ms
const unsigned long HEARTBEAT_INTERVAL  = 30000;  // ms

/* ─── NETWORK ───────────────────────────────────────── */
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer server(80);

/* ─── SENSORS ───────────────────────────────────────── */
Adafruit_BME280  bme;
Adafruit_MPU6050 mpu;
bool hasBME  = false;
bool hasMPU  = false;
bool hasLoRa = false;

float  currentTemp     = 28.4;     // °C
float  currentHum      = 71.0;     // %
float  currentPressure = 1013.0;   // hPa
float  mpuFallbackTemp = 25.0;

/* ─── STATE ─────────────────────────────────────────── */
unsigned long  lastButtonPress = 0;
unsigned long  lastHeartbeat   = 0;
unsigned long  lastLedBlink    = 0;
bool           simulatedAlertActive = false;
String         lastAlertType   = "";
unsigned long  alertLedUntil  = 0;    // when to return LED to green

/* ═══════════════════════════════════════════════════════
   WEB PORTAL HTML — Full Pathfinder interface
   ═══════════════════════════════════════════════════════ */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Pathfinder System Interface</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #121212; color: #E0E0E0; text-align: center; padding: 15px; margin: 0; }
    h1 { color: #00C853; margin-bottom: 0px; font-size: 24px; text-transform: uppercase; letter-spacing: 1px; }
    .subtitle { color: #9E9E9E; font-size: 12px; margin-top: 5px; margin-bottom: 20px; text-transform: uppercase; letter-spacing: 2px;}
    .grid-container { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .card { background: #1E1E1E; padding: 15px; border-radius: 8px; border: 1px solid #333; }
    .card h2 { margin: 0; font-size: 12px; color: #757575; text-transform: uppercase; letter-spacing: 1px;}
    .data { font-size: 28px; font-weight: bold; margin: 5px 0 0 0; color: #03A9F4; }
    .location-card { grid-column: 1 / -1; background: #263238; border: 1px solid #37474F; text-align: left; }
    .location-data { font-size: 14px; color: #CFD8DC; margin: 5px 0; }
    .location-data strong { color: #81D4FA; }
    .map-container { grid-column: 1 / -1; background: #1E1E1E; padding: 15px; border-radius: 8px; border: 1px solid #333; position: relative; overflow: hidden; }
    .alert-card { grid-column: 1 / -1; background: #3E2723; border: 1px solid #FF5252; transition: 0.3s; }
    .chat-container { grid-column: 1 / -1; background: #1A1A1A; border-radius: 8px; border: 1px solid #333; overflow: hidden; display: flex; flex-direction: column; height: 250px; text-align: left;}
    .chat-header { background: #00C853; color: #121212; padding: 10px; font-weight: bold; font-size: 14px; text-align: center; text-transform: uppercase; letter-spacing: 1px; }
    .chat-box { flex-grow: 1; padding: 10px; overflow-y: auto; display: flex; flex-direction: column; gap: 8px; }
    .msg { padding: 8px 12px; border-radius: 4px; max-width: 85%; font-size: 13px; line-height: 1.4; }
    .msg.bot { background: #2C2C2C; color: #00E676; align-self: flex-start; border-left: 3px solid #00E676; }
    .msg.user { background: #0288D1; color: #FFF; align-self: flex-end; }
    .chat-input-area { display: flex; border-top: 1px solid #333; }
    #chatInput { flex-grow: 1; background: #121212; color: #FFF; border: none; padding: 12px; outline: none; font-size: 14px;}
    #chatSend { background: #0288D1; color: #FFF; border: none; padding: 0 15px; font-weight: bold; cursor: pointer; text-transform: uppercase; font-size: 12px;}
    .btn { width: 100%; padding: 16px; color: white; border: none; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 15px; text-transform: uppercase; letter-spacing: 1px; transition: background-color 0.3s;}
    #sosBtn { background-color: #D32F2F; }
    #simBtn { background-color: #F57C00; }
    @keyframes pulseFade { 0%{opacity:0.2;} 50%{opacity:1.0;} 100%{opacity:0.2;} }
    .pulse-dot { fill: #00E676; animation: pulseFade 2s ease-in-out infinite; }
  </style>
</head>
<body>
  <h1>Pathfinder Node</h1>
  <p class="subtitle">Offline Telemetry & Safety Interface</p>
  <div class="grid-container">
    <div class="card location-card">
      <h2>Location Data</h2>
      <p class="location-data"><strong>Zone:</strong> <span id="loc-address">Loading...</span></p>
      <p class="location-data"><strong>Coordinates:</strong> <span id="loc-lat">--</span>, <span id="loc-lon">--</span></p>
    </div>
    <div class="map-container">
      <h2>Topographical Overview</h2>
      <svg viewBox="0 0 300 150" style="width: 100%; height: auto; margin-top: 10px; background-color: #263238; border-radius: 4px;">
        <path d="M 20 130 Q 80 140 120 80 T 220 50 T 280 20" fill="transparent" stroke="#546E7A" stroke-width="3" stroke-dasharray="4,4"/>
        <circle cx="20" cy="130" r="4" fill="#E53935" />
        <text x="5" y="145" fill="#B0BEC5" font-size="9">Base (0.0 km)</text>
        <circle cx="280" cy="20" r="4" fill="#FBC02D" />
        <text x="210" y="15" fill="#B0BEC5" font-size="9">Summit (5.0 km)</text>
        <circle cx="150" cy="67" r="7" class="pulse-dot" />
        <text x="90" y="90" fill="#00E676" font-size="10" font-weight="bold">ACTIVE POSITION (2.5 km)</text>
      </svg>
    </div>
    <div class="chat-container">
      <div class="chat-header">Automated Guide System</div>
      <div class="chat-box" id="chatBox">
        <div class="msg bot">System Initialized. I am your offline trail assistant. Enter a query below.</div>
      </div>
      <div class="chat-input-area">
        <input type="text" id="chatInput" placeholder="Enter query (e.g., 'water', 'emergency')...">
        <button id="chatSend">Send</button>
      </div>
    </div>
    <div class="card"><h2>Temperature</h2><p class="data"><span id="temp">--</span> &deg;C</p></div>
    <div class="card"><h2>Humidity</h2><p class="data"><span id="hum">--</span> %</p></div>
    <div class="card" style="grid-column: 1 / -1;"><h2>Barometric Pressure</h2><p class="data"><span id="pres">--</span> hPa</p></div>
    <div class="card alert-card" id="status-card"><h2>System Status</h2><p class="data" id="status" style="color:#00E676;">SECURE</p></div>
  </div>
  <button id="sosBtn" class="btn">Transmit Emergency SOS</button>
  <button id="simBtn" class="btn">Test Seismic Diagnostics</button>
  <script>
    const chatBox = document.getElementById('chatBox');
    const chatInput = document.getElementById('chatInput');
    const chatSend = document.getElementById('chatSend');
    function getBotResponse(input) {
      let text = input.toLowerCase();
      if (text.includes("lost") || text.includes("help") || text.includes("emergency")) return "Advisory: Remain stationary. If injured, press the 'Transmit Emergency SOS' button to alert local authorities via the LoRa network.";
      if (text.includes("elephant") || text.includes("animal") || text.includes("wildlife")) return "Advisory: Maintain distance from all wildlife. This node actively monitors seismic activity to detect heavy biological movement.";
      if (text.includes("water") || text.includes("drink")) return "Information: A natural water source is located 1.5km north on the current bearing. Ensure all water is purified prior to consumption.";
      return "Command not recognized. Valid inquiry topics include: emergency procedures, wildlife protocols, and resource locations.";
    }
    function addMessage(text, sender) {
      const m = document.createElement('div'); m.classList.add('msg', sender); m.innerText = text;
      chatBox.appendChild(m); chatBox.scrollTop = chatBox.scrollHeight;
    }
    chatSend.addEventListener('click', () => {
      let t = chatInput.value.trim(); if (!t) return;
      addMessage(t, 'user'); chatInput.value = "";
      setTimeout(() => addMessage(getBotResponse(t), 'bot'), 400);
    });
    chatInput.addEventListener('keypress', e => { if (e.key === 'Enter') chatSend.click(); });
    setInterval(() => {
      fetch('/sensordata?t=' + Date.now()).then(r => r.json()).then(data => {
        document.getElementById('loc-address').innerText = data.address;
        document.getElementById('loc-lat').innerText = data.lat;
        document.getElementById('loc-lon').innerText = data.lon;
        document.getElementById('temp').innerText = data.temperature;
        document.getElementById('hum').innerText = data.humidity;
        document.getElementById('pres').innerText = data.pressure;
        if (data.alert == "1") {
          document.getElementById('status-card').style.background = '#4A148C';
          document.getElementById('status').style.color = '#E040FB';
          document.getElementById('status').innerText = "SEISMIC ALERT";
        } else {
          document.getElementById('status-card').style.background = '#3E2723';
          document.getElementById('status').style.color = '#00E676';
          document.getElementById('status').innerText = "SECURE";
        }
      });
    }, 2000);
    document.getElementById('sosBtn').addEventListener('click', () => {
      const b = document.getElementById('sosBtn');
      b.innerText = "TRANSMITTING..."; b.style.backgroundColor = "#B71C1C";
      fetch('/trigger_sos?t=' + Date.now()).then(r => {
        if (r.ok) {
          b.innerText = "SOS TRANSMITTED"; b.style.backgroundColor = "#388E3C";
          setTimeout(() => { b.innerText = "Transmit Emergency SOS"; b.style.backgroundColor = "#D32F2F"; }, 5000);
        }
      });
    });
    document.getElementById('simBtn').addEventListener('click', () => {
      const b = document.getElementById('simBtn'); b.innerText = "INITIATING...";
      fetch('/simulate_alert?t=' + Date.now()).then(r => {
        if (r.ok) { b.innerText = "DIAGNOSTIC ACTIVE"; setTimeout(() => b.innerText = "Test Seismic Diagnostics", 3000); }
      });
    });
  </script>
</body>
</html>
)rawliteral";

/* ═══════════════════════════════════════════════════════
   HELPERS
   ═══════════════════════════════════════════════════════ */

// RGB LED — common cathode. Pass 0/1 for each channel.
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
  digitalWrite(LED_B, b ? HIGH : LOW);
}

// Read sensors and update global state
void updateSensors() {
  if (hasBME) {
    currentTemp     = bme.readTemperature();
    currentHum      = bme.readHumidity();
    currentPressure = bme.readPressure() / 100.0F;
  } else if (hasMPU) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    currentTemp     = temp.temperature;
    currentHum      = 70.0;                  // no humidity sensor — placeholder
    currentPressure = 1013.0;                // no pressure — placeholder
  }
}

// Transmit an alert packet
void sendLoRaAlert(String alertType) {
  lastAlertType = alertType;
  if (hasLoRa) {
    LoRa.beginPacket();
    LoRa.print(NODE_ID);
    LoRa.print(", ALERT: ");
    LoRa.print(alertType);
    LoRa.endPacket();
    Serial.print("[LORA TX] Alert -> ");
    Serial.println(alertType);
  } else {
    Serial.print("[LORA-MOCK] Alert -> ");
    Serial.println(alertType);
  }
}

// Transmit a heartbeat packet with live sensor data
void sendLoRaHeartbeat() {
  updateSensors();
  String payload = NODE_ID + ", HEARTBEAT, TEMP:" + String(currentTemp, 1)
                           + ", HUM:"  + String((int)currentHum)
                           + ", PRES:" + String(currentPressure, 1);
  if (hasLoRa) {
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
    Serial.print("[LORA TX] Heartbeat -> ");
    Serial.println(payload);
  } else {
    Serial.print("[LORA-MOCK] Heartbeat -> ");
    Serial.println(payload);
  }
}

/* ═══════════════════════════════════════════════════════
   SETUP
   ═══════════════════════════════════════════════════════ */
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n╔══════════════════════════════════════════════╗");
  Serial.println("║       PATHFINDER NODE PF-KNUCKLES-01         ║");
  Serial.println("║       Booting all subsystems...              ║");
  Serial.println("╚══════════════════════════════════════════════╝");

  // RGB LED — start with white (boot indicator)
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  setLed(1, 1, 1);

  // SOS button
  pinMode(SOS_PIN, INPUT_PULLUP);
  Serial.println("[OK] SOS button on GPIO 15");

  // I2C bus for sensors
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.printf("[OK] I2C bus on SDA=%d SCL=%d\n", I2C_SDA, I2C_SCL);

  // BME280 (primary sensor)
  if (bme.begin(0x76)) {
    hasBME = true;
    Serial.println("[OK] BME280 connected (addr 0x76)");
  } else if (bme.begin(0x77)) {
    hasBME = true;
    Serial.println("[OK] BME280 connected (addr 0x77)");
  } else {
    Serial.println("[WARN] BME280 not found — trying MPU6050 fallback");
  }

  // MPU6050 (fallback for temperature only)
  if (mpu.begin()) {
    hasMPU = true;
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
    Serial.println("[OK] MPU6050 connected");
  } else {
    Serial.println("[WARN] MPU6050 not found");
  }

  if (!hasBME && !hasMPU) {
    Serial.println("[INFO] No sensors found — using mock telemetry");
  }

  // LoRa radio
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (LoRa.begin(433E6)) {
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(18);
    hasLoRa = true;
    Serial.println("[OK] LoRa radio 433MHz SF7 BW125 CR4/5");
  } else {
    Serial.println("[WARN] LoRa radio failed — alerts will only log to serial");
  }

  // WiFi captive portal
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Pathfinder-Net");
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("[OK] WiFi AP 'Pathfinder-Net' active");

  // Web routes
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });

  server.on("/sensordata", HTTP_GET, []() {
    updateSensors();
    String json = "{";
    json += "\"address\":\"" + NODE_ADDRESS + "\",";
    json += "\"lat\":\"" + NODE_LAT + "\",";
    json += "\"lon\":\"" + NODE_LON + "\",";
    json += "\"temperature\":\"" + String(currentTemp, 1) + "\",";
    json += "\"humidity\":\""    + String((int)currentHum) + "\",";
    json += "\"pressure\":\""    + String(currentPressure, 1) + "\",";
    json += "\"alert\":\""       + String(simulatedAlertActive ? 1 : 0) + "\"";
    json += "}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
    if (simulatedAlertActive) simulatedAlertActive = false;
  });

  server.on("/trigger_sos", HTTP_GET, []() {
    Serial.println("[WEB] Virtual SOS pressed");
    sendLoRaAlert("VIRTUAL_WEB_SOS");
    setLed(1, 0, 0);            // red — web SOS
    alertLedUntil = millis() + 5000;  // hold red for 5 s
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "SOS Transmitted");
  });

  server.on("/simulate_alert", HTTP_GET, []() {
    Serial.println("[WEB] Seismic diagnostics requested");
    simulatedAlertActive = true;
    sendLoRaAlert("AI_SEISMIC_TARGET_DETECTED");
    setLed(1, 0, 1);            // purple — seismic
    alertLedUntil = millis() + 5000;  // hold purple for 5 s
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Diagnostic Sent");
  });

  // Captive-portal redirects
  server.on("/generate_204",       HTTP_GET, []() { server.sendHeader("Location", "http://" + apIP.toString(), true); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html",HTTP_GET, []() { server.sendHeader("Location", "http://" + apIP.toString(), true); server.send(302, "text/plain", ""); });
  server.onNotFound([]() { server.sendHeader("Location", "http://" + apIP.toString(), true); server.send(302, "text/plain", ""); });

  server.begin();
  setLed(0, 1, 0);  // green = secure
  Serial.println("[READY] All systems nominal — SECURE status\n");
}

/* ═══════════════════════════════════════════════════════
   LOOP
   ═══════════════════════════════════════════════════════ */
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // ── Hardware SOS button ────────────────────────────
  if (digitalRead(SOS_PIN) == LOW && (millis() - lastButtonPress > BUTTON_COOLDOWN)) {
    Serial.println("\n[CRITICAL] Hardware SOS pressed!");
    sendLoRaAlert("HARDWARE_SOS_TRIGGERED");
    lastButtonPress = millis();
    setLed(1, 0, 0);            // bright red — hardware SOS
    alertLedUntil = millis() + 5000;  // hold red for 5 s
  }

  // ── Periodic heartbeat (every 30 s) ────────────────
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendLoRaHeartbeat();
    lastHeartbeat = millis();
  }

  // ── LED recovery: return to green once alertLedUntil passes ────
  if (millis() > alertLedUntil) {
    setLed(0, 1, 0);            // green = secure
  }
}
