#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>

// --- ESP32 LoRa Pins ---
#define LORA_SS    15
#define LORA_RST   4
#define LORA_DIO0  2

// --- Hardware Button ---
#define SOS_BUTTON_PIN 14
unsigned long lastButtonPress = 0;
const unsigned long buttonCooldown = 2000;

// --- Network Settings ---
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer server(80);

// --- HTML Interface ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Pathfinder SOS Portal</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background:#121212; color:#E0E0E0; text-align:center; padding:20px; margin:0; }
    h1 { color:#00C853; font-size:24px; text-transform:uppercase; letter-spacing:1px; }
    p  { color:#9E9E9E; font-size:14px; margin-bottom:30px; }
    .btn { width:100%; padding:20px; color:white; border:none; border-radius:8px; font-size:18px; font-weight:bold; cursor:pointer; text-transform:uppercase; letter-spacing:2px; }
    #sosBtn  { background:#D32F2F; box-shadow:0 4px 15px rgba(211,47,47,0.4); }
    #status  { margin-top:20px; font-weight:bold; color:#00E676; }
  </style>
</head>
<body>
  <h1>Pathfinder Node</h1>
  <p>Emergency Telemetry Interface</p>
  <button id="sosBtn" class="btn">Transmit Virtual SOS</button>
  <div id="status">SYSTEM SECURE</div>
  <script>
    document.getElementById('sosBtn').addEventListener('click', function() {
      var btn = document.getElementById('sosBtn');
      var status = document.getElementById('status');
      btn.innerText = "TRANSMITTING...";
      btn.style.backgroundColor = "#B71C1C";
      status.innerText = "SENDING ALERT TO BASE...";
      status.style.color = "#FF5252";
      fetch('/trigger_sos?t=' + new Date().getTime()).then(function(response) {
        if (response.ok) {
          btn.innerText = "SOS TRANSMITTED";
          btn.style.backgroundColor = "#388E3C";
          status.innerText = "BASE NOTIFIED";
          status.style.color = "#00C853";
          setTimeout(function() {
            btn.innerText = "Transmit Virtual SOS";
            btn.style.backgroundColor = "#D32F2F";
            status.innerText = "SYSTEM SECURE";
            status.style.color = "#00E676";
          }, 5000);
        }
      });
    });
  </script>
</body>
</html>
)rawliteral";

// --- Heartbeat timing ---
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000; // 30 seconds

void setup() {
  Serial.begin(115200);
  // FIX 1: Removed while(!Serial) — it blocks forever in the field with no USB connection.
  // Serial.begin() alone is sufficient on the ESP32.
  delay(100); // brief stabilisation delay instead

  Serial.println("\n--- Pathfinder Node: Dual SOS & Wi-Fi Active ---");

  // Setup hardware button
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);

  // Setup LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("[ERROR] LoRa failed. Check wiring!");
    while (true) delay(10); // Only safe to block here — LoRa is non-negotiable
  }

  // FIX 2: Explicitly set LoRa parameters — must match base.ino exactly.
  LoRa.setSpreadingFactor(7);       // SF7
  LoRa.setSignalBandwidth(125E3);   // 125 kHz
  LoRa.setCodingRate4(5);           // CR 4/5
  LoRa.setTxPower(18);
  Serial.println("[SUCCESS] LoRa Radio Ready — 433MHz SF7 BW125 CR4/5");

  // Setup Wi-Fi Captive Portal
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Pathfinder-Net");
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("[SUCCESS] Wi-Fi Hotspot 'Pathfinder-Net' active.");

  // Web server endpoints
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/trigger_sos", HTTP_GET, []() {
    Serial.println("\n[ALERT] Web UI Virtual SOS Pressed!");
    sendLoRaAlert("VIRTUAL_WEB_SOS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "SOS Transmitted");
  });

  // Captive portal redirects
  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Physical SOS button (active LOW with INPUT_PULLUP)
  if (digitalRead(SOS_BUTTON_PIN) == LOW && (millis() - lastButtonPress > buttonCooldown)) {
    Serial.println("\n[ALERT] Physical SOS Button Pressed!");
    sendLoRaAlert("HARDWARE_SOS_TRIGGERED");
    lastButtonPress = millis();
  }

  // FIX 3: Periodic heartbeat with sensor data so dashboard stays live.
  // Replace the dummy values below with your real sensor readings.
  if (millis() - lastHeartbeat > heartbeatInterval) {
    float temperature = 28.4; // TODO: replace with real sensor read
    int   humidity    = 71;   // TODO: replace with real sensor read
    sendLoRaHeartbeat(temperature, humidity);
    lastHeartbeat = millis();
  }
}

// Send an alert packet
void sendLoRaAlert(String alertType) {
  Serial.print("[TX] LoRa Alert: ");
  Serial.println(alertType);
  LoRa.beginPacket();
  LoRa.print("PF-KNUCKLES-01, ALERT: " + alertType);
  LoRa.endPacket();
}

// Send a heartbeat packet so the dashboard updates temp/humidity
void sendLoRaHeartbeat(float temp, int hum) {
  String payload = "PF-KNUCKLES-01, HEARTBEAT, TEMP:" + String(temp, 1) + ", HUM:" + String(hum);
  Serial.print("[TX] LoRa Heartbeat: ");
  Serial.println(payload);
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
}
