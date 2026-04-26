#include <SPI.h>
#include <LoRa.h>

// --- Arduino Uno SPI pins ---
#define LORA_SS   10
#define LORA_RST   9
#define LORA_DIO0  2

void setup() {
  Serial.begin(115200);
  while (!Serial); // Safe here — Uno serial IS the USB to the laptop

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("{\"error\": \"LoRa init failed. Check wiring and 3.3V power.\"}");
    while (1);
  }

  // FIX 1: Explicit LoRa parameters — must match node.ino exactly.
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("{\"status\": \"Base Station Active. Listening on 433MHz SF7 BW125...\"}");
}

// FIX 2: Escape a string for safe JSON embedding.
// Replaces backslash and double-quote so the JSON output never breaks.
String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  return s;
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // FIX 2 applied: escape payload before embedding in JSON string
    String safePayload = jsonEscape(incoming);

    // FIX 3: Include SNR so the dashboard can display a real value
    Serial.print("{\"type\": \"alert\", \"payload\": \"");
    Serial.print(safePayload);
    Serial.print("\", \"rssi\": ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", \"snr\": ");
    Serial.print(LoRa.packetSnr());
    Serial.println("}");
  }
}
