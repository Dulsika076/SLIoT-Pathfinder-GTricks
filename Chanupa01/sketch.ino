#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <LoRa.h>

// --- Custom I2C Pin Definitions ---
#define I2C_SDA 32
#define I2C_SCL 33

// --- LoRa Pin Definitions (ESP32) ---
#define LORA_SS    15 
#define LORA_RST   16 
#define LORA_DIO0  0  

// --- Hardware Button Definition ---
#define SOS_BUTTON_PIN 14 
unsigned long lastButtonPress = 0;
const unsigned long buttonCooldown = 2000; 

// --- Network Settings (Adapted for Wokwi) ---
WebServer server(80);

// --- Node Specific Location Data ---
const String NODE_ID = "PF-KNUCKLES-01";
const String NODE_LAT = "7.4116 N";
const String NODE_LON = "80.8060 E";
const String NODE_ADDRESS = "Pekoe Trail, Stage 2 - Knuckles Mountain Range";

// --- Sensor Objects & Hardware Flags ---
Adafruit_MPU6050 mpu;
Adafruit_BME280 bme; 

bool hasMPU = false;
bool hasBME = false;
bool hasLoRa = false;
bool simulatedAlertActive = false; 
unsigned long lastAlertTime = 0;

// --- Edge AI Mock Variables ---
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE 100
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t feature_ix = 0;
unsigned long lastSampleTime = 0;
bool simulateElephant = false; 
float sim_time = 0.0;
float baseline_Z = 0.0;
float mpu_temperature = 0.0; 

// --- HTML Interface (Stored in PROGMEM) ---
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
    @keyframes pulseFade { 0% { opacity: 0.2; } 50% { opacity: 1.0; } 100% { opacity: 0.2; } }
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
      if (text.includes("lost") || text.includes("help") || text.includes("emergency")) {
        return "Advisory: Remain stationary. If injured, press the 'Transmit Emergency SOS' button to alert local authorities via the LoRa network.";
      } else if (text.includes("elephant") || text.includes("animal") || text.includes("wildlife")) {
        return "Advisory: Maintain distance from all wildlife. This node actively monitors seismic activity to detect heavy biological movement.";
      } else if (text.includes("water") || text.includes("drink")) {
        return "Information: A natural water source is located 1.5km north on the current bearing. Ensure all water is purified prior to consumption.";
      } else {
        return "Command not recognized. Valid inquiry topics include: emergency procedures, wildlife protocols, and resource locations.";
      }
    }

    function addMessage(text, sender) {
      const msgDiv = document.createElement('div');
      msgDiv.classList.add('msg', sender);
      msgDiv.innerText = text;
      chatBox.appendChild(msgDiv);
      chatBox.scrollTop = chatBox.scrollHeight;
    }

    chatSend.addEventListener('click', () => {
      let userText = chatInput.value.trim();
      if (userText === "") return;
      addMessage(userText, 'user');
      chatInput.value = "";
      setTimeout(() => { addMessage(getBotResponse(userText), 'bot'); }, 400);
    });

    chatInput.addEventListener('keypress', function (e) {
      if (e.key === 'Enter') chatSend.click();
    });

    setInterval(function() {
      fetch('/sensordata?t=' + new Date().getTime()).then(response => response.json()).then(data => {
          document.getElementById('loc-address').innerText = data.address;
          document.getElementById('loc-lat').innerText = data.lat;
          document.getElementById('loc-lon').innerText = data.lon;
          document.getElementById('temp').innerText = data.temperature;
          document.getElementById('hum').innerText = data.humidity;
          document.getElementById('pres').innerText = data.pressure;
          
          if(data.alert == "1") {
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

    fetch('/sensordata?t=' + new Date().getTime()).then(response => response.json()).then(data => {
        document.getElementById('loc-address').innerText = data.address;
        document.getElementById('loc-lat').innerText = data.lat;
        document.getElementById('loc-lon').innerText = data.lon;
    });

    document.getElementById('sosBtn').addEventListener('click', function() {
      const btn = document.getElementById('sosBtn');
      btn.innerText = "TRANSMITTING...";
      btn.style.backgroundColor = "#B71C1C"; 

      fetch('/trigger_sos?t=' + new Date().getTime()).then(response => {
          if(response.ok) {
            btn.innerText = "SOS TRANSMITTED";
            btn.style.backgroundColor = "#388E3C"; 
            setTimeout(() => { 
              btn.innerText = "Transmit Emergency SOS"; 
              btn.style.backgroundColor = "#D32F2F"; 
            }, 5000);
          }
      }).catch(err => {
          btn.innerText = "TRANSMISSION FAILED";
          setTimeout(() => { 
              btn.innerText = "Transmit Emergency SOS"; 
              btn.style.backgroundColor = "#D32F2F"; 
          }, 3000);
      });
    });

    document.getElementById('simBtn').addEventListener('click', function() {
      const btn = document.getElementById('simBtn');
      btn.innerText = "INITIATING...";
      
      fetch('/simulate_alert?t=' + new Date().getTime()).then(response => {
          if(response.ok) {
            btn.innerText = "DIAGNOSTIC ACTIVE";
            setTimeout(() => { btn.innerText = "Test Seismic Diagnostics"; }, 3000);
          }
      });
    });
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[INIT] Starting Pathfinder Edge AI Node...");

  // --- Initialize Custom I2C Bus ---
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("[INIT] I2C Bus re-routed to SDA: 32, SCL: 33.");

  // --- Hardware Button Initialization ---
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("[INIT] Physical SOS Button configured on GPIO 14.");
  
  // --- Sensor Initialization ---
  if (!mpu.begin()) { 
    Serial.println("[WARN] MPU6050 unavailable. Initiating synthetic AI mode."); 
    hasMPU = false;
  } else { 
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ); 
    hasMPU = true;
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    baseline_Z = a.acceleration.z;
    mpu_temperature = temp.temperature; 
    Serial.println("[INIT] MPU6050 connected and calibrated.");
  }
  
  if (!bme.begin(0x76)) { 
    Serial.println("[WARN] BME280 unavailable. Initiating mock telemetry mode."); 
    hasBME = false;
  } else { 
    hasBME = true; 
    Serial.println("[INIT] BME280 connected.");
  }

  // --- LoRa Initialization ---
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { 
    Serial.println("[WARN] LoRa module failed. Transmissions routed to Serial only."); 
    hasLoRa = false;
  } else { 
    hasLoRa = true; 
    Serial.println("[INIT] LoRa module connected.");
  }

  // --- Wi-Fi Station Setup for Wokwi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.print("[INIT] Connecting to Wokwi WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\n[SYS] Network Connected!");
  Serial.print("[SYS] Click here to open Web UI: http://");
  Serial.println(WiFi.localIP());

  // --- Web Server Endpoints ---
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  
  server.on("/sensordata", HTTP_GET, []() {
    String json = "{";
    json += "\"address\":\"" + NODE_ADDRESS + "\",";
    json += "\"lat\":\"" + NODE_LAT + "\",";
    json += "\"lon\":\"" + NODE_LON + "\",";
    
    if (hasBME) {
        json += "\"temperature\":\"" + String(bme.readTemperature(), 1) + "\",";
    } else if (hasMPU) {
        json += "\"temperature\":\"" + String(mpu_temperature, 1) + "\",";
    } else {
        json += "\"temperature\":\"" + String(24.0 + random(0, 30)/10.0, 1) + "\","; 
    }

    json += "\"humidity\":\"" + String(hasBME ? bme.readHumidity() : (60.0 + random(0, 10)), 0) + "\",";
    json += "\"pressure\":\"" + String(hasBME ? (bme.readPressure() / 100.0F) : (1010.0 + random(-5, 5)), 1) + "\",";
    json += "\"alert\":\"" + String(simulatedAlertActive ? 1 : 0) + "\"";
    json += "}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
    if (simulatedAlertActive) simulatedAlertActive = false; 
  });

  server.on("/trigger_sos", HTTP_GET, []() {
    Serial.println("[EVENT] Web UI: Virtual SOS Triggered.");
    sendLoRaAlert("VIRTUAL_SOS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "SOS Transmitted");
  });

  server.on("/simulate_alert", HTTP_GET, []() {
    Serial.println("[EVENT] Diagnostics requested: Injecting synthetic target signature.");
    simulateElephant = true;
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Alert Triggered");
  });

  server.begin();
  Serial.println("[SYS] Web Server Running.");
}

void loop() {
  server.handleClient();

  // --- TELEMETRY DEBUG PRINT ---
  static unsigned long lastTelemetryPrint = 0;
  if (millis() - lastTelemetryPrint > 2000) {
      if (hasMPU) {
          Serial.print("[TELEMETRY] MPU6050 Temperature: ");
          Serial.print(mpu_temperature);
          Serial.println(" C");
      }
      lastTelemetryPrint = millis();
  }

  // --- HARDWARE SOS BUTTON LOGIC ---
  int buttonReading = digitalRead(SOS_BUTTON_PIN);
  
  if (buttonReading == LOW && (millis() - lastButtonPress > buttonCooldown)) {
      Serial.println("\n[CRITICAL EVENT] Physical SOS Button Actuated!");
      sendLoRaAlert("HARDWARE_SOS_TRIGGERED");
      lastButtonPress = millis(); 
  }

  // --- MOCKED INFERENCE PIPELINE (Runs at 100Hz) ---
  if (millis() - lastSampleTime >= 10) {
    lastSampleTime = millis();
    
    if (hasMPU) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        mpu_temperature = temp.temperature;
    }

    // Trigger alert immediately if "Simulate Diagnostic" was pressed
    if (simulateElephant) {
        if (millis() - lastAlertTime > 5000) {
            Serial.println("\n[ALERT] Simulated Target Signature Detected! (Mock Mode)");
            simulatedAlertActive = true;
            sendLoRaAlert("AI_SEISMIC_TARGET_DETECTED");
            simulateElephant = false; 
            lastAlertTime = millis();
        }
    }
  }
}

void sendLoRaAlert(String alertType) {
  if (hasLoRa) {
    LoRa.beginPacket();
    LoRa.print(NODE_ID);
    LoRa.print(", ALERT: ");
    LoRa.print(alertType);
    LoRa.endPacket();
    Serial.print("[LORA] Packet Transmitted -> ");
    Serial.println(alertType);
  } else {
    Serial.print("[LORA-MOCK] TX Event Logged -> ");
    Serial.println(alertType);
  }
}