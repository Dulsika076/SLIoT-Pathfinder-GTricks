#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// --- Custom I2C Pin Definitions ---
#define I2C_SDA 32
#define I2C_SCL 33

Adafruit_BME280 bme; // Create the BME280 object

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for the Serial console to open
  }

  Serial.println("\n--- BME280 Isolated Hardware Diagnostic ---");

  // 1. Start the I2C bus on your custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("I2C Bus started on Pins 32 (SDA) and 33 (SCL).");

  // 2. Attempt to initialize the BME280 sensor
  // Note: Most BME280 modules use address 0x76. Some use 0x77.
  if (!bme.begin(0x77)) {
    Serial.println("\n[ERROR] Could not find a valid BME280 sensor!");
    Serial.println("Troubleshooting Checklist:");
    Serial.println("- Are SDA (32) and SCL (33) wired correctly?");
    Serial.println("- Is the sensor receiving 3.3V power?");
    Serial.println("- Try changing the address in the code from 0x76 to 0x77.");
    while (1) {
      delay(10); // Freeze here if initialization fails
    }
  }

  Serial.println("\n[SUCCESS] BME280 Found and Initialized Perfectly!\n");
}

void loop() {
  // Read data from the sensor
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F; // Convert Pa to hPa

  // Print the data to the Serial Monitor
  Serial.print("[ENVIRONMENT] ");
  Serial.print("Temp: ");
  Serial.print(temperature, 2);
  Serial.print(" C  |  ");
  
  Serial.print("Humidity: ");
  Serial.print(humidity, 2);
  Serial.print(" %  |  ");
  
  Serial.print("Pressure: ");
  Serial.print(pressure, 2);
  Serial.println(" hPa");

  // Wait 2 seconds before the next reading
  delay(2000); 
}