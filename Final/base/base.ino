/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  PATHFINDER BASE STATION — Final Firmware
 *  Hardware: Arduino Uno + RA-02 LoRa (433 MHz)
 *  Role: Receive LoRa packets, forward as JSON over USB serial to Python
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <SPI.h>
#include <LoRa.h>

/* ─── Arduino Uno hardware SPI pins ─── */
#define LORA_SS    10
#define LORA_RST    9
#define LORA_DIO0   2

void setup() {
  Serial.begin(115200);
  while (!Serial);           // Safe on Uno — serial IS the USB link

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("{\"error\": \"LoRa init failed. Check wiring & 3.3V power.\"}");
    while (1);
  }

  // Must match node.ino parameters exactly
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("{\"status\": \"Base Station Active. Listening 433MHz SF7 BW125...\"}");
}

/*
 * Escape a string for safe embedding in a JSON string literal.
 * Replaces \ with \\ and " with \"
 */
String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  return s;
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();

    String safe = jsonEscape(incoming);

    Serial.print("{\"type\": \"alert\", \"payload\": \"");
    Serial.print(safe);
    Serial.print("\", \"rssi\": ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", \"snr\": ");
    Serial.print(LoRa.packetSnr());
    Serial.println("}");
  }
}
