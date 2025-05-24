/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */

#include "Display_SPD2010.h"
#include "Audio_PCM5101.h"
#include "RTC_PCF85063.h"
#include "Gyro_QMI8658.h"
#include "LVGL_Driver.h"
//#include "MIC_MSM.h"
#include "PWR_Key.h"
#include "SD_Card.h"
#include "LVGL_Example.h"
#include "BAT_Driver.h"
#include "ui.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include <time.h>
#include <ArduinoJson.h>
#include "ESP_I2S.h"

#define SAMPLE_RATE      44100
#define BITS_PER_SAMPLE  24        // Actual sample precision saved to file
#define CHANNELS         1         // Mono
#define RECORD_TIME_SEC  10        // 10-second recording
#define BUFFER_SIZE 1024

#define I2S_PIN_BCK   15
#define I2S_PIN_WS    2
#define I2S_PIN_DOUT  -1    // Not used for recording
#define I2S_PIN_DIN   39    // Microphone data input

I2SClass i2s;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600; // UTC+3
const int daylightOffset_sec = 0;

const char* SSID = "powerpuffgirls";
const char* PASSWD = "powerpuffgirls";
const char* ROUTE = "/api/data";
const char* UPLOAD = "/upload";
uint32_t lastFileHash = 0;

struct SamplingSlot
{
  int fromMinutes;
  int toMinutes;
  int intervalMinutes;
};

std::vector<SamplingSlot> currentSamplingSlots;

TaskHandle_t dynamicRecordingHandle = NULL;

uint32_t calculateFileCRC32(File &file)
{
  const size_t bufSize = 512;
  uint8_t buffer[bufSize];
  uint32_t crc = 0xFFFFFFFF;

  while (file.available())
  {
    size_t len = file.read(buffer, bufSize);
    for (size_t i = 0; i < len; ++i)
    {
      crc ^= buffer[i];
      for (int j = 0; j < 8; j++)
      {
        if (crc & 1)
          crc = (crc >> 1) ^ 0xEDB88320;
        else
          crc >>= 1;
      }
    }
  }

  return ~crc;
}

int parseTimeStrToMinutes(const String &timeStr)
{
  int hour = 0, min = 0;
  sscanf(timeStr.c_str(), "%d:%d", &hour, &min);
  return hour * 60 + min;
}

void loadSamplingSchedule()
{
  currentSamplingSlots.clear();

  File file = SD_MMC.open("/samplingRateConfig.txt", FILE_READ);
  if (!file)
  {
    Serial.println("ERROR: Couldn't open samplingRateConfig.txt");
    return;
  }

  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    int i1 = line.indexOf(',');
    int i2 = line.indexOf(',', i1 + 1);
    if (i1 == -1 || i2 == -1)
      continue;

    String fromStr = line.substring(0, i1);
    String toStr = line.substring(i1 + 1, i2);
    String level = line.substring(i2 + 1);

    SamplingSlot slot;
    slot.fromMinutes = parseTimeStrToMinutes(fromStr);
    slot.toMinutes = parseTimeStrToMinutes(toStr);

    if (level.equalsIgnoreCase("low"))
      slot.intervalMinutes = 30;
    else if (level.equalsIgnoreCase("medium"))
      slot.intervalMinutes = 15;
    else if (level.equalsIgnoreCase("high"))
      slot.intervalMinutes = 1;
    else
      continue;

    currentSamplingSlots.push_back(slot);
  }

  file.close();
  Serial.println("Sampling schedule loaded.");
}

void writeWavHeader(File &file, uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channels, uint32_t dataSize) {
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  uint16_t blockAlign = channels * bitsPerSample / 8;
  uint32_t chunkSize = 36 + dataSize;

  file.write((const uint8_t *)"RIFF", 4);
  file.write((uint8_t *)&chunkSize, 4);
  file.write((const uint8_t *)"WAVE", 4);
  file.write((const uint8_t *)"fmt ", 4);

  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;  // PCM
  file.write((uint8_t *)&subchunk1Size, 4);
  file.write((uint8_t *)&audioFormat, 2);
  file.write((uint8_t *)&channels, 2);
  file.write((uint8_t *)&sampleRate, 4);
  file.write((uint8_t *)&byteRate, 4);
  file.write((uint8_t *)&blockAlign, 2);
  file.write((uint8_t *)&bitsPerSample, 2);

  file.write((const uint8_t *)"data", 4);
  file.write((uint8_t *)&dataSize, 4);
}

void recordAudio() {
  Serial.println("=== Starting Audio Recording ===");

  String filename = "/recording_" + String(datetime.hour) + "_" +
                    String(datetime.minute) + "_" +
                    String(datetime.second) + ".wav";

  uint32_t totalSamples = SAMPLE_RATE * RECORD_TIME_SEC;
  uint32_t dataSize = totalSamples * 3;  // 24-bit = 3 bytes/sample

  File wavFile = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!wavFile) {
    Serial.println("ERROR: Failed to create WAV file");
    return;
  }

  writeWavHeader(wavFile, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS, dataSize);

  i2s_chan_handle_t rx_handle = i2s.rxChan();
  if (!rx_handle) {
    Serial.println("ERROR: RX channel handle is NULL");
    wavFile.close();
    return;
  }

  i2s_channel_disable(rx_handle);
  vTaskDelay(pdMS_TO_TICKS(100));
  if (i2s_channel_enable(rx_handle) != ESP_OK) {
    Serial.println("ERROR: Failed to enable I2S channel");
    wavFile.close();
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(100));

  const size_t bufferSize = BUFFER_SIZE * 4; // 32-bit samples
  uint8_t* buffer = (uint8_t*)malloc(bufferSize);
  if (!buffer) {
    Serial.println("ERROR: Failed to allocate recording buffer");
    wavFile.close();
    return;
  }
  Serial.printf("Recording at %d Hz, %d-bit, %s\n",
              i2s.rxSampleRate(),
              (int)i2s.rxDataWidth(),
              i2s.rxSlotMode() == I2S_SLOT_MODE_MONO ? "Mono" : "Stereo");
  Serial.println("Recording...");
  uint32_t totalBytesWritten = 0;

  while (totalBytesWritten < dataSize) {
    size_t bytesToRead = min((size_t)(dataSize - totalBytesWritten) * 4 / 3, bufferSize);
    size_t bytesRead = 0;

    esp_err_t err = i2s_channel_read(rx_handle, buffer, bytesToRead, &bytesRead, pdMS_TO_TICKS(1000));
    if (err != ESP_OK || bytesRead == 0) {
      Serial.println("ERROR: I2S read failed");
      break;
    }

    int32_t* samples = (int32_t*)buffer;
    size_t sampleCount = bytesRead / 4;

    for (size_t i = 0; i < sampleCount && totalBytesWritten < dataSize; ++i) {
      int32_t sample = samples[i];
      uint8_t pcm24[3];
      pcm24[0] = (sample >> 8) & 0xFF;
      pcm24[1] = (sample >> 16) & 0xFF;
      pcm24[2] = (sample >> 24) & 0xFF;
      wavFile.write(pcm24, 3);
      totalBytesWritten += 3;
    }
  }

  i2s_channel_disable(rx_handle);
  free(buffer);
  wavFile.close();

  // Verify file was created
  File testFile = SD_MMC.open(filename.c_str(), FILE_READ);
  if (testFile) {
    size_t fileSize = testFile.size();
    testFile.close();
    Serial.printf("WAV file created: %s (%d bytes)\n", filename.c_str(), fileSize);
    
    // Copy to the expected filename for upload
    if (SD_MMC.exists("/myfile.wav")) {
      SD_MMC.remove("/myfile.wav");
    }
    
    // Simple file copy
    File source = SD_MMC.open(filename.c_str(), FILE_READ);
    File dest = SD_MMC.open("/myfile.wav", FILE_WRITE);
    if (source && dest) {
      while (source.available()) {
        dest.write(source.read());
      }
      source.close();
      dest.close();
      Serial.println("File copied to /myfile.wav for upload");
    }
  } else {
    Serial.printf("ERROR: Failed to verify file creation: %s\n", filename.c_str());
  }

  Serial.printf("Recording complete: %d bytes written\n", totalBytesWritten);
}

void sendAudio() {
  String url = "http://192.168.137.194:12000/upload";

  //String url = "http://" + WiFi.gatewayIP().toString() + ":12000" + UPLOAD;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    return;
  }

  File wavFile = SD_MMC.open("/myfile.wav", FILE_READ);
  if (!wavFile) {
    Serial.println("ERROR: failed to open /myfile.wav");
    return;
  }

  size_t fileSize = wavFile.size();
  Serial.printf("Sending WAV file, size: %u bytes\n", fileSize);

  if (fileSize <= 44) { // Just the WAV header, no actual audio data
    Serial.println("ERROR: File is too small - likely no audio data recorded");
    wavFile.close();
    return;
  }

  WiFiClient client;
  HTTPClient http;
  
  Serial.printf("Sending audio file to %s\n", url.c_str());
  http.begin(client, url.c_str());

  // Create multipart form data
  String boundary = "----ESP32Boundary1234567890";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);

  // Calculate content length
  String header = "--" + boundary + "\r\n";
  header += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  header += "Content-Type: audio/wav\r\n\r\n";
  String footer = "\r\n--" + boundary + "--\r\n";
  
  size_t totalLength = header.length() + fileSize + footer.length();

  client.connect("192.168.137.194", 12000);  // Remove http://

// Send HTTP POST request headers
client.print("POST /upload HTTP/1.1\r\n");
client.print("Host: 192.168.137.194\r\n");
client.print("Content-Type: " + contentType + "\r\n");
client.print("Content-Length: " + String(totalLength) + "\r\n");
client.print("\r\n");
  
  // Start the request
  /*client.connect("http://192.168.137.194", 12000);
  client.print("POST " + String(UPLOAD) + " HTTP/1.1\r\n");
  client.print("Host:http://192.168.137.194:12000\r\n");
  client.print("Content-Type: " + contentType + "\r\n");
  
  client.print("\r\n");*/
  
  // Send multipart header
  client.print(header);
  
  // Send file data in chunks
  const size_t chunkSize = 1024;
  uint8_t buffer[chunkSize];
  size_t totalSent = 0;
  
  while (wavFile.available()) {
    size_t bytesRead = wavFile.read(buffer, min(chunkSize, (size_t)wavFile.available()));
    size_t bytesSent = client.write(buffer, bytesRead);
    totalSent += bytesSent;
    
    if (bytesSent != bytesRead) {
      Serial.printf("WARNING: Sent %d bytes, expected %d\n", bytesSent, bytesRead);
    }
  }
  
  // Send multipart footer
  client.print(footer);
  
  wavFile.close();
  
  Serial.printf("Sent %d bytes total\n", totalSent);
  
  // Read response
  String response = "";
  unsigned long timeout = millis() + 5000; // 5 second timeout
  
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      response += client.readString();
      break;
    }
    delay(10);
  }
  
  http.end();
  client.stop();
  Serial.println("Response:");
  Serial.println(response);
}

// Add this function to analyze the recorded audio data
void analyzeAudioData() {
  File wavFile = SD_MMC.open("/myfile.wav", FILE_READ);
  if (!wavFile) {
    Serial.println("ERROR: Cannot open WAV file for analysis");
    return;
  }

  size_t fileSize = wavFile.size();
  Serial.printf("=== Audio Analysis ===\n");
  Serial.printf("File size: %d bytes\n", fileSize);

  if (fileSize <= 44) {
    Serial.println("File only contains WAV header, no audio data");
    wavFile.close();
    return;
  }

  // Skip WAV header (44 bytes)
  wavFile.seek(44);
  
  // Read first 1000 samples (2000 bytes for 16-bit)
  const int samplesToAnalyze = 1000;
  int16_t samples[samplesToAnalyze];
  size_t bytesRead = wavFile.read((uint8_t*)samples, samplesToAnalyze * 2);
  
  if (bytesRead == 0) {
    Serial.println("No audio data found after header");
    wavFile.close();
    return;
  }

  // Analyze the samples
  int16_t minVal = 32767, maxVal = -32768;
  int32_t sum = 0;
  int nonZeroCount = 0;
  int zeroCount = 0;

  for (int i = 0; i < bytesRead / 2; i++) {
    int16_t sample = samples[i];
    
    if (sample != 0) {
      nonZeroCount++;
      if (sample < minVal) minVal = sample;
      if (sample > maxVal) maxVal = sample;
    } else {
      zeroCount++;
    }
    
    sum += abs(sample);
  }

  float avgAmplitude = (float)sum / (bytesRead / 2);
  
  Serial.printf("Samples analyzed: %d\n", bytesRead / 2);
  Serial.printf("Zero samples: %d\n", zeroCount);
  Serial.printf("Non-zero samples: %d\n", nonZeroCount);
  Serial.printf("Min value: %d\n", minVal);
  Serial.printf("Max value: %d\n", maxVal);
  Serial.printf("Average amplitude: %.2f\n", avgAmplitude);
  
  // Print first 20 samples as hex and decimal
  Serial.println("First 20 samples (hex | decimal):");
  for (int i = 0; i < min(20, (int)(bytesRead / 2)); i++) {
    Serial.printf("  %04X | %6d\n", (uint16_t)samples[i], samples[i]);
  }

  // Check if data looks like valid audio
  if (zeroCount == bytesRead / 2) {
    Serial.println("*** ALL SAMPLES ARE ZERO - No audio signal ***");
  } else if (nonZeroCount < (bytesRead / 20)) {
    Serial.println("*** VERY FEW NON-ZERO SAMPLES - Weak or no signal ***");
  } else if (maxVal - minVal < 100) {
    Serial.println("*** VERY LOW DYNAMIC RANGE - Signal may be too quiet ***");
  } else {
    Serial.println("Audio data appears to contain signal");
  }

  wavFile.close();
}

// Test function to check I2S data in real-time
void testI2SDataLive() {
  Serial.println("=== Live I2S Data Test ===");
  
  i2s_chan_handle_t rx_handle = i2s.rxChan();
  if (!rx_handle) {
    Serial.println("ERROR: RX handle is NULL");
    return;
  }

  // Reset channel state
  i2s_channel_disable(rx_handle);
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_err_t err = i2s_channel_enable(rx_handle);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Cannot enable channel: %s\n", esp_err_to_name(err));
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(100)); // Let channel settle

  const int testSamples = 100;
  int16_t buffer[testSamples];
  
  Serial.println("Reading live data for 5 seconds...");
  for (int test = 0; test < 5; test++) {
    size_t bytesRead = 0;
    err = i2s_channel_read(rx_handle, (uint8_t*)buffer, testSamples * 2, &bytesRead, pdMS_TO_TICKS(1000));
    
    if (err != ESP_OK) {
      Serial.printf("Read error: %s\n", esp_err_to_name(err));
      continue;
    }

    if (bytesRead == 0) {
      Serial.println("No data read");
      continue;
    }

    // Analyze this chunk
    int16_t minVal = 32767, maxVal = -32768;
    int nonZero = 0;
    for (int i = 0; i < bytesRead / 2; i++) {
      if (buffer[i] != 0) {
        nonZero++;
        if (buffer[i] < minVal) minVal = buffer[i];
        if (buffer[i] > maxVal) maxVal = buffer[i];
      }
    }

    Serial.printf("Test %d: %d bytes, %d non-zero, range %d to %d | First few: ", 
                  test + 1, bytesRead, nonZero, minVal, maxVal);
    
    for (int i = 0; i < min(5, (int)(bytesRead / 2)); i++) {
      Serial.printf("%d ", buffer[i]);
    }
    Serial.println();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  i2s_channel_disable(rx_handle);
  Serial.println("Live test complete");
}

// Test different I2S configurations
void testI2SConfigurations() {
  Serial.println("=== Testing I2S Configurations ===");
  
  // Test 1: Current configuration
  Serial.println("Test 1: Current configuration (MONO, 16-bit, 16kHz)");
  testI2SDataLive();
  
  // Test 2: Try stereo mode
  Serial.println("\nTest 2: Trying STEREO mode");
  i2s.end();
  vTaskDelay(pdMS_TO_TICKS(500));
  
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
  bool success = i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  if (success) {
    success = i2s.configureRX(SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, I2S_RX_TRANSFORM_NONE);
    if (success) {
      testI2SDataLive();
    } else {
      Serial.println("Failed to configure RX in stereo mode");
    }
  } else {
    Serial.println("Failed to begin I2S in stereo mode");
  }
  
  // Restore original configuration
  Serial.println("\nRestoring original configuration");
  setupI2S();
}

// Enhanced microphone test with different settings
void testMicrophoneSettings() {
  Serial.println("=== Microphone Settings Test ===");
  
  // Test with different bit depths and sample rates
  struct TestConfig {
    uint32_t sampleRate;
    i2s_data_bit_width_t bitWidth;
    const char* description;
  };
  
  TestConfig configs[] = {
    {8000, I2S_DATA_BIT_WIDTH_16BIT, "8kHz 16-bit"},
    {16000, I2S_DATA_BIT_WIDTH_16BIT, "16kHz 16-bit"},
    {44100, I2S_DATA_BIT_WIDTH_16BIT, "44.1kHz 16-bit"},
    {16000, I2S_DATA_BIT_WIDTH_32BIT, "16kHz 32-bit"}
  };
  
  for (int i = 0; i < 4; i++) {
    Serial.printf("\nTesting: %s\n", configs[i].description);
    
    i2s.end();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
    bool success = i2s.begin(I2S_MODE_STD, configs[i].sampleRate, configs[i].bitWidth, I2S_SLOT_MODE_MONO);
    
    if (success) {
      success = i2s.configureRX(configs[i].sampleRate, configs[i].bitWidth, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE);
      if (success) {
        // Quick test read
        i2s_chan_handle_t rx_handle = i2s.rxChan();
        if (rx_handle) {
          i2s_channel_disable(rx_handle);
          vTaskDelay(pdMS_TO_TICKS(50));
          if (i2s_channel_enable(rx_handle) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
            
            uint8_t buffer[1024];
            size_t bytesRead;
            esp_err_t err = i2s_channel_read(rx_handle, buffer, 1024, &bytesRead, pdMS_TO_TICKS(1000));
            
            Serial.printf("  Result: %s, bytes: %d\n", esp_err_to_name(err), bytesRead);
            
            if (bytesRead > 0 && configs[i].bitWidth == I2S_DATA_BIT_WIDTH_16BIT) {
              int16_t* samples = (int16_t*)buffer;
              int nonZero = 0;
              for (int j = 0; j < bytesRead / 2; j++) {
                if (samples[j] != 0) nonZero++;
              }
              Serial.printf("  Non-zero samples: %d/%d\n", nonZero, bytesRead / 2);
            }
            
            i2s_channel_disable(rx_handle);
          }
        }
      } else {
        Serial.println("  Failed to configure RX");
      }
    } else {
      Serial.println("  Failed to begin I2S");
    }
  }
  
  // Restore original setup
  Serial.println("\nRestoring original configuration");
  setupI2S();
}

bool readCredentials(String &primary, String &secondary)
{
  File file = SD_MMC.open("/credentials.txt", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open credentials.txt");
    return false;
  }

  primary = file.readStringUntil('\n');
  primary.trim();
  secondary = file.readStringUntil('\n');
  secondary.trim();
  file.close();
  return true;
}

String readFilteringConfigJSON()
{
  File file = SD_MMC.open("/filteringConfig.txt", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open filteringConfig.txt");
    return "{}";
  }
  String json = "\"filters\":{";
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();
    int idx = line.lastIndexOf(',');
    if (idx == -1)
      continue;

    String key = line.substring(0, idx);
    String val = line.substring(idx + 1);
    key.replace("\"", "\\\""); // Escape quotes if any
    json += "\"" + key + "\":" + val + ",";
  }
  file.close();

  if (json.endsWith(","))
    json.remove(json.length() - 1);
  json += "}";

  return json;
}

void dynamicRecordingTask(void *pvParameters)
{
  while (1)
  {
    // Make sure datetime is valid
    if (datetime.hour >= 24 || datetime.minute >= 60)
    {
      Serial.println("Invalid datetime; retrying in 1 minute.");
      vTaskDelay(pdMS_TO_TICKS(60000));
      continue;
    }

    int nowMins = datetime.hour * 60 + datetime.minute;

    // Default interval is "medium"
    int delayMinutes = 15;

    for (auto &slot : currentSamplingSlots)
    {
      if (nowMins >= slot.fromMinutes && nowMins < slot.toMinutes)
      {
        delayMinutes = slot.intervalMinutes;
        break;
      }
    }

    Serial.printf("Recording now. Next recording in %d minutes.\n", delayMinutes);

    recordAudio();
    Serial.printf("Recording done\n");
    sendAudio();

    vTaskDelay(pdMS_TO_TICKS(delayMinutes * 60000));
  }
}

void sendAudio()
{
  String url = "http://" + WiFi.gatewayIP().toString() + ":12000" + AUDIOROUTE;
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected.");
  }
  else
  {
    WiFiClient client;
    HTTPClient http;
    File wavFile;
    size_t fileSize;
    int code;

    Serial.printf("Sending audio file to %s\n", url.c_str());
    http.begin(client, url.c_str());
    http.addHeader("Content-Type", "audio/wav");

    wavFile = SD_MMC.open("/myfile.wav", FILE_READ);
    if (!wavFile)
    {
      Serial.println("ERROR: failed to open /myfile.wav");
    }
    else
    {
      fileSize = wavFile.size();
      Serial.printf("Sending WAV file, size: %u bytes\n", fileSize);

      // sendRequest has an overload that takes a Stream*
      code = http.sendRequest("POST", &wavFile, fileSize);
      Serial.printf("[HTTP] POST code: %d\n", code);

      if (code > 0)
      {
        String response = http.getString();
        Serial.println("Response payload:");
        Serial.println(response);
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, response);

        if (error)
        {
          Serial.print("JSON parse failed: ");
          Serial.println(error.c_str());
          return;
        }

        const char *emotion = doc["emotion_prediction"];
        const char *age = doc["age_prediction"];
        const char *timestamp = doc["timestamp"];
        // the following part is missing
        const char *gender = doc["gender_prediction"];

        // add call GSM function if result is ANOMALI

        Serial.printf("Emotion: %s\n", emotion);
        Serial.printf("Age: %s\n", age);
        Serial.printf("Timestamp: %s\n", timestamp);

        load_table_data(emotion, age, gender, timestamp, result);
      }
      wavFile.close();
    }
    http.end(); // always close the connection
  }
}
// load the results to user interface table
void load_table_data(const char *timestamp, const char * age, const char *gender, const char *emotion, const char *result)
{
  File file = SD_MMC.open("/results.txt", FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.printf("%s,%s,%s,%s,%s\n", timestamp, age, gender, emotion, result);
  file.close();
  Serial.println("Appended new prediction data.");
}

void sendConfig()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    String primary, secondary;
    if (!readCredentials(primary, secondary))
      return;

    String filtersJson = readFilteringConfigJSON();

    // Construct JSON payload
    String payload = "{";
    payload += "\"primaryPhone\":\"" + primary + "\",";
    payload += "\"secondaryPhone\":\"" + secondary + "\",";
    payload += filtersJson;
    payload += "}";

    WiFiClient client;
    HTTPClient http;
    String url = "http://192.168.137.194:12000/api/data";
    http.begin(client, url.c_str());
    http.addHeader("Content-Type", "application/json");
    Serial.printf("Target URL: %s\n", url.c_str());

    int code = http.POST(payload);

    Serial.printf("[HTTP] POST code: %d\n", code);
    if (code > 0)
      Serial.println(http.getString());

    http.end(); // VERY important!
  }
  else
  {
    Serial.println("WiFi not connected.");
  }
}

void sendDataTask(void *pvParameters)
{
  while (1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      String primary, secondary;
      if (!readCredentials(primary, secondary))
        return;

      String filtersJson = readFilteringConfigJSON();

      // Construct JSON payload
      String payload = "{";
      payload += "\"primaryPhone\":\"" + primary + "\",";
      payload += "\"secondaryPhone\":\"" + secondary + "\",";
      payload += filtersJson;
      payload += "}";

      WiFiClient client;
      HTTPClient http;
      String url = "http://" + WiFi.gatewayIP().toString() + ":12000" + ROUTE;
      http.begin(client, url.c_str());
      http.addHeader("Content-Type", "application/json");
      Serial.printf("Target URL: %s\n", url.c_str());

      int code = http.POST(payload);

      Serial.printf("[HTTP] POST code: %d\n", code);
      if (code > 0)
        Serial.println(http.getString());

      http.end(); // VERY important!
    }
    else
    {
      Serial.println("WiFi not connected.");
    }

    vTaskDelay(pdMS_TO_TICKS(5000)); // for debugging, reduce interval
  }
}

void configWatcherTask(void *param)
{
  while (1)
  {
    File f = SD_MMC.open("/samplingRateConfig.txt", FILE_READ);
    if (f)
    {
      uint32_t currentHash = calculateFileCRC32(f);
      if (currentHash != lastFileHash)
      {
        Serial.println("Detected change in samplingRateConfig.txt.");
        lastFileHash = currentHash;
        RestartDynamicRecordingTask();
      }
      f.close();
    }
    else
    {
      Serial.println("Failed to open samplingRateConfig.txt for monitoring.");
    }

    vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10 sec
  }
}

void RestartDynamicRecordingTask()
{
  if (dynamicRecordingHandle)
  {
    vTaskDelete(dynamicRecordingHandle);
    dynamicRecordingHandle = NULL;
    Serial.println("Old recording task killed.");
  }

  loadSamplingSchedule();

  xTaskCreatePinnedToCore(
      dynamicRecordingTask,
      "DynamicRecording",
      8192,
      NULL,
      1,
      &dynamicRecordingHandle,
      0 // Core 0
  );

  Serial.println("Recording task restarted.");
}

void rtcDriverTask(void *param)
{
  while (true)
  {
    PCF85063_Read_Time(&datetime);   // Update global datetime for UI
    vTaskDelay(pdMS_TO_TICKS(1000)); // Update every 1 sec
  }
}

void setupI2S() {
  Serial.println("=== I2S Setup ===");
  
  // Configure I2S pins
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
  i2s.setTimeout(1000);

  // Initialize I2S in standard mode for recording
  bool success = i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  if (!success) {
    Serial.println("ERROR: I2S begin failed!");
    Serial.printf("Last I2S error: %d\n", i2s.lastError());
    while (1) {
      delay(1000);
      Serial.println("I2S initialization failed - system halted");
    }
  }
  
  Serial.println("I2S begin successful");

  // Configure RX channel for recording
  success = i2s.configureRX(SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE);
  if (!success) {
    Serial.println("ERROR: I2S configureRX failed!");
    Serial.printf("Last I2S error: %d\n", i2s.lastError());
    while (1) {
      delay(1000);
      Serial.println("I2S RX configuration failed - system halted");
    }
  }
  
  Serial.println("I2S RX configuration successful");

  // Verify RX channel handle exists
  i2s_chan_handle_t rx_handle = i2s.rxChan();
  if (!rx_handle) {
    Serial.println("ERROR: RX channel handle is NULL after configuration.");
    while (1) {
      delay(1000);
      Serial.println("I2S RX handle NULL - system halted");
    }
  }
  
  Serial.printf("I2S RX Handle: %p\n", rx_handle);
  
  // Ensure channel starts in disabled state
  esp_err_t err = i2s_channel_disable(rx_handle);
  Serial.printf("Initial channel disable: %s\n", esp_err_to_name(err));
  
  // Brief delay
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Test enable/disable cycle
  err = i2s_channel_enable(rx_handle);
  if (err == ESP_OK) {
    Serial.println("I2S channel test enable successful");
    vTaskDelay(pdMS_TO_TICKS(50));
    
    err = i2s_channel_disable(rx_handle);
    Serial.printf("I2S channel test disable: %s\n", esp_err_to_name(err));
  } else {
    Serial.printf("WARNING: I2S channel enable test failed: %s\n", esp_err_to_name(err));
  }
  
  Serial.println("I2S setup complete - ready for recording");
  Serial.printf("I2S Target Sample Rate: %d Hz\n", SAMPLE_RATE);
  Serial.printf("I2S Actual RX Sample Rate: %f Hz\n", i2s.rxSampleRate());
  Serial.printf("I2S Actual RX Bit Width: %d-bit\n", (int)i2s.rxDataWidth());
  Serial.printf("I2S Actual RX Slot Mode: %s\n", i2s.rxSlotMode() == I2S_SLOT_MODE_MONO ? "Mono" : "Stereo");
}

void setupI2S() {
  Serial.println("=== I2S Setup ===");
  
  // Configure I2S pins
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
  i2s.setTimeout(1000);

  // Initialize I2S in standard mode for recording
  bool success = i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  if (!success) {
    Serial.println("ERROR: I2S begin failed!");
    Serial.printf("Last I2S error: %d\n", i2s.lastError());
    while (1) {
      delay(1000);
      Serial.println("I2S initialization failed - system halted");
    }
  }
  
  Serial.println("I2S begin successful");

  // Configure RX channel for recording
  success = i2s.configureRX(SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE);
  if (!success) {
    Serial.println("ERROR: I2S configureRX failed!");
    Serial.printf("Last I2S error: %d\n", i2s.lastError());
    while (1) {
      delay(1000);
      Serial.println("I2S RX configuration failed - system halted");
    }
  }
  
  Serial.println("I2S RX configuration successful");

  // Verify RX channel handle exists
  i2s_chan_handle_t rx_handle = i2s.rxChan();
  if (!rx_handle) {
    Serial.println("ERROR: RX channel handle is NULL after configuration.");
    while (1) {
      delay(1000);
      Serial.println("I2S RX handle NULL - system halted");
    }
  }
  
  Serial.printf("I2S RX Handle: %p\n", rx_handle);
  
  // Ensure channel starts in disabled state
  esp_err_t err = i2s_channel_disable(rx_handle);
  Serial.printf("Initial channel disable: %s\n", esp_err_to_name(err));
  
  // Brief delay
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // Test enable/disable cycle
  err = i2s_channel_enable(rx_handle);
  if (err == ESP_OK) {
    Serial.println("I2S channel test enable successful");
    vTaskDelay(pdMS_TO_TICKS(50));
    
    err = i2s_channel_disable(rx_handle);
    Serial.printf("I2S channel test disable: %s\n", esp_err_to_name(err));
  } else {
    Serial.printf("WARNING: I2S channel enable test failed: %s\n", esp_err_to_name(err));
  }
  
  Serial.println("I2S setup complete - ready for recording");
  Serial.printf("I2S Target Sample Rate: %d Hz\n", SAMPLE_RATE);
  Serial.printf("I2S Actual RX Sample Rate: %f Hz\n", i2s.rxSampleRate());
  Serial.printf("I2S Actual RX Bit Width: %d-bit\n", (int)i2s.rxDataWidth());
  Serial.printf("I2S Actual RX Slot Mode: %s\n", i2s.rxSlotMode() == I2S_SLOT_MODE_MONO ? "Mono" : "Stereo");
}

void Driver_Init()
{
  Flash_test();
  PWR_Init();
  BAT_Init();
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Backlight_Init();
  Set_Backlight(50); // 0~100
  PCF85063_Init();
  // QMI8658_Init();
}
void setup()
{
  Serial.begin(115200);
  // Check PSRAM
  size_t ps = esp_psram_get_size();
  size_t sp = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf(">> PSRAM size: %u  SPIRAM free: %u\n", ps, sp);

  WiFi.begin(SSID, PASSWD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    datetime.year = timeinfo.tm_year + 1900;
    datetime.month = timeinfo.tm_mon + 1;
    datetime.day = timeinfo.tm_mday;
    datetime.dotw = timeinfo.tm_wday;
    datetime.hour = timeinfo.tm_hour;
    datetime.minute = timeinfo.tm_min;
    datetime.second = timeinfo.tm_sec;

    PCF85063_Set_All(datetime);
    Serial.println("RTC set from NTP and global datetime updated.");
  }

  Driver_Init();
  setupI2S();
  SD_Init();
  //Audio_Init();
  //MIC_Init();
  LCD_Init();
  Lvgl_Init();
  File f = SD_MMC.open("/samplingRateConfig.txt", FILE_READ);
  if (f) {
    lastFileHash = calculateFileCRC32(f);
    f.close();
  }
  sendConfig();
  RestartDynamicRecordingTask();
  ui_init();

  xTaskCreatePinnedToCore(
      configWatcherTask,
      "ConfigWatcher",
      4096,
      NULL,
      1,
      NULL,
      0 // Core 0
  );
  xTaskCreatePinnedToCore(
      rtcDriverTask,
      "RTC Driver",
      2048,
      NULL,
      1,
      NULL,
      0 // Core 0
  );
}
void loop()
{
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}
