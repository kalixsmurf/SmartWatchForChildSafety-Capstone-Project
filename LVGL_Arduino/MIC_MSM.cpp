#include "MIC_MSM.h"

void MIC_Init() {
  // Set up I2S pins and configurations
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
  i2s.setTimeout(1000);

  // Initialize I2S in standard mode
  bool success = i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  if (!success) {
    Serial.println("I2S begin failed!");
    while (1); // Halt or retry
  }

  i2s_chan_handle_t rx_handle = i2s.rxChan();
  if (!rx_handle) {
    Serial.println("RX channel handle is NULL.");
    while (true);
  }

  //Force disable first
  esp_err_t err = i2s_channel_disable(rx_handle);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("RX disable failed: %s\n", esp_err_to_name(err));
  }

  //Now enable again
  err = i2s_channel_enable(rx_handle);
  if (err == ESP_OK) {
    Serial.println("RX channel re-enabled successfully.");
  } else {
    Serial.printf("Failed to enable RX channel: %s\n", esp_err_to_name(err));
    while (true);
  }
}
