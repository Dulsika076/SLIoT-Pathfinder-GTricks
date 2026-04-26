#include <driver/i2s.h>

// --- Custom I2S Pin Definitions ---
#define I2S_SCK 4
#define I2S_WS  5
#define I2S_SD  18

// We use I2S Port 0 on the ESP32
#define I2S_PORT I2S_NUM_0

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("\n--- INMP441 I2S Microphone Diagnostic ---");

  // 1. Configure the I2S Audio Driver
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // ESP32 is master, receiving data
    .sample_rate = 16000,                                // 16kHz is perfect for voice/environmental noise
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,        // INMP441 outputs 24-bit data, so we read 32 and shift
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // Because we tied the L/R pin to Ground
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Standard I2S protocol
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // Standard interrupt
    .dma_buf_count = 8,                                  // Number of audio buffers
    .dma_buf_len = 64,                                   // Size of each buffer
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // 2. Map the I2S driver to our custom pins
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, // Not transmitting audio out
    .data_in_num = I2S_SD
  };

  // 3. Install the driver and start the hardware
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.println("[ERROR] Failed to install I2S driver!");
    while(1) delay(10);
  }
  
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.println("[ERROR] Failed to set I2S pins!");
    while(1) delay(10);
  }

  Serial.println("[SUCCESS] INMP441 Initialized. Starting audio stream...");
  delay(1000);
}

void loop() {
  int32_t sample = 0;
  size_t bytesIn = 0;

  // Read a single 32-bit audio sample directly from the DMA buffer
  esp_err_t result = i2s_read(I2S_PORT, &sample, sizeof(int32_t), &bytesIn, portMAX_DELAY);

  if (result == ESP_OK && bytesIn > 0) {
    // The INMP441 provides 24-bit data, but the ESP32 reads it into a 32-bit container.
    // We bit-shift it 8 spaces to the right to clean out the empty bits and get the true amplitude.
    sample >>= 8;

    // Print raw audio amplitude to the serial bus
    Serial.println(sample);
  }
}