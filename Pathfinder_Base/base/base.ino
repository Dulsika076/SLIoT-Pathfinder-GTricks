#include <SPI.h>
#include <LoRa.h>

// --- Uno SPI Pin Definitions ---
#define LORA_SS 10
#define LORA_RST 9
#define LORA_DIO0 2

void setup() {
  // Set to 115200 to match the Python laptop script
  Serial.begin(115200);
  while (!Serial);

  // Tell the LoRa library which pins the Uno is using
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Initialize the radio at 433 MHz
  if (!LoRa.begin(433E6)) {
    // Print error in JSON format so Python understands it
    Serial.println("{\"error\": \"LoRa init failed. Check wiring and 3.3V power.\"}");
    while (1); 
  }
  
  // Print success status in JSON format
  Serial.println("{\"status\": \"Base Station Active. Listening on 433 MHz...\"}");
}

void loop() {
  // Check if a radio packet has arrived from the forest
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    String incoming = "";
    
    // Read the packet
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    
    // Output clean JSON over the USB cable for Python to catch
    Serial.print("{\"type\": \"alert\", \"payload\": \"");
    Serial.print(incoming);
    Serial.print("\", \"rssi\": ");
    Serial.print(LoRa.packetRssi());
    Serial.println("}");
  }
}