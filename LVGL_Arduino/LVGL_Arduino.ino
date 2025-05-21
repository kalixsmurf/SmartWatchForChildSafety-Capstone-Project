/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */

#include "Display_SPD2010.h"
#include "Audio_PCM5101.h"
#include "RTC_PCF85063.h"
#include "Gyro_QMI8658.h"
#include "LVGL_Driver.h"
#include "MIC_MSM.h"
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

#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define CHANNELS 1
#define RECORD_SECONDS 10
#define BUFFER_SIZE 1024

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;  //UTC+3
const int daylightOffset_sec = 0;

const char* SSID = "DESKTOP-JQQF5JK";
const char* PASSWD = ",Er592301";
const char* ROUTE = "/api/data";
const char* AUDIOROUTE = "/upload";
uint32_t lastFileHash = 0;

struct SamplingSlot {
  int fromMinutes;
  int toMinutes;
  int intervalMinutes;
};

std::vector<SamplingSlot> currentSamplingSlots;

TaskHandle_t dynamicRecordingHandle = NULL;

uint32_t calculateFileCRC32(File& file) {
  const size_t bufSize = 512;
  uint8_t buffer[bufSize];
  uint32_t crc = 0xFFFFFFFF;

  while (file.available()) {
    size_t len = file.read(buffer, bufSize);
    for (size_t i = 0; i < len; ++i) {
      crc ^= buffer[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1)
          crc = (crc >> 1) ^ 0xEDB88320;
        else
          crc >>= 1;
      }
    }
  }

  return ~crc;
}

int parseTimeStrToMinutes(const String& timeStr) {
  int hour = 0, min = 0;
  sscanf(timeStr.c_str(), "%d:%d", &hour, &min);
  return hour * 60 + min;
}

void loadSamplingSchedule() {
  currentSamplingSlots.clear();

  File file = SD_MMC.open("/samplingRateConfig.txt", FILE_READ);
  if (!file) {
    Serial.println("ERROR: Couldn't open samplingRateConfig.txt");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int i1 = line.indexOf(',');
    int i2 = line.indexOf(',', i1 + 1);
    if (i1 == -1 || i2 == -1) continue;

    String fromStr = line.substring(0, i1);
    String toStr = line.substring(i1 + 1, i2);
    String level = line.substring(i2 + 1);

    SamplingSlot slot;
    slot.fromMinutes = parseTimeStrToMinutes(fromStr);
    slot.toMinutes = parseTimeStrToMinutes(toStr);

    if (level.equalsIgnoreCase("low")) slot.intervalMinutes = 30;
    else if (level.equalsIgnoreCase("medium")) slot.intervalMinutes = 15;
    else if (level.equalsIgnoreCase("high")) slot.intervalMinutes = 1;
    else continue;

    currentSamplingSlots.push_back(slot);
  }

  file.close();
  Serial.println("Sampling schedule loaded.");
}

/* void writeWavHeader(File &file, int sampleRate, int bitsPerSample, int channels, int dataSize) {
  int byteRate = sampleRate * channels * bitsPerSample / 8;
  int blockAlign = channels * bitsPerSample / 8;
  int chunkSize = 36 + dataSize;

  file.write((const uint8_t*)"RIFF", 4);
  file.write((uint8_t*)&chunkSize, 4);
  file.write((const uint8_t*)"WAVE", 4);
  file.write((const uint8_t*)"fmt ", 4);

  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;

  file.write((uint8_t*)&subchunk1Size, 4);
  file.write((uint8_t*)&audioFormat, 2);
  file.write((uint8_t*)&channels, 2);
  file.write((uint8_t*)&sampleRate, 4);
  file.write((uint8_t*)&byteRate, 4);
  file.write((uint8_t*)&blockAlign, 2);
  file.write((uint8_t*)&bitsPerSample, 2);

  file.write((const uint8_t*)"data", 4);
  file.write((uint8_t*)&dataSize, 4);
} */

void recordAudio() {
  Serial.println("Audio recording initializing...");

  /* // 1. Calculate total data size
  int totalBytesToRecord = SAMPLE_RATE * RECORD_SECONDS * (BITS_PER_SAMPLE / 8);
  int bytesRecorded = 0;
  uint8_t buffer[BUFFER_SIZE];

  // 2. Open file and write WAV header
  File file = SD.open("/micTest.wav", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open micTest.wav for writing.");
    return;
  }
 // writeWavHeader(file, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS, totalBytesToRecord);

  // 3. Read from I2S and write to file
  while (bytesRecorded < totalBytesToRecord) {
    int bytesToRead = min(BUFFER_SIZE, totalBytesToRecord - bytesRecorded);
    //int bytesRead = i2s.read((int16_t*)buffer, bytesToRead / 2);
    if (bytesRead > 0) {
      file.write(buffer, bytesRead);
      bytesRecorded += bytesRead;
    } else {
      Serial.println("I2S read timeout");
    }
  }

  file.close();
  Serial.println("Recording complete and saved to /micTest.wav"); */
}

bool readCredentials(String& primary, String& secondary) {
  File file = SD_MMC.open("/credentials.txt", FILE_READ);
  if (!file) {
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

String readFilteringConfigJSON() {
  File file = SD_MMC.open("/filteringConfig.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open filteringConfig.txt");
    return "{}";
  }
  String json = "\"filters\":{";
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    int idx = line.lastIndexOf(',');
    if (idx == -1) continue;

    String key = line.substring(0, idx);
    String val = line.substring(idx + 1);
    key.replace("\"", "\\\"");  // Escape quotes if any
    json += "\"" + key + "\":" + val + ",";
  }
  file.close();

  if (json.endsWith(",")) json.remove(json.length() - 1);
  json += "}";

  return json;
}

void dynamicRecordingTask(void* pvParameters) {
  while (1) {
    // Make sure datetime is valid
    if (datetime.hour >= 24 || datetime.minute >= 60) {
      Serial.println("Invalid datetime; retrying in 1 minute.");
      vTaskDelay(pdMS_TO_TICKS(60000));
      continue;
    }

    int nowMins = datetime.hour * 60 + datetime.minute;

    // Default interval is "medium"
    int delayMinutes = 15;

    for (auto& slot : currentSamplingSlots) {
      if (nowMins >= slot.fromMinutes && nowMins < slot.toMinutes) {
        delayMinutes = slot.intervalMinutes;
        break;
      }
    }

    Serial.printf("Recording now. Next recording in %d minutes.\n", delayMinutes);

    //recordAudio();
    Serial.printf("Recording done\n");
    sendAudio();

    vTaskDelay(pdMS_TO_TICKS(delayMinutes * 60000));
  }
}

void sendAudio() {
  String url = "http://" + WiFi.gatewayIP().toString() + ":12000" + ROUTE;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
  } else {
    WiFiClient client;
    HTTPClient http;
    File wavFile;
    size_t fileSize;
    int code;

    Serial.printf("Sending audio file to %s\n", url.c_str());
    http.begin(client, url.c_str());
    http.addHeader("Content-Type", "audio/wav");

    wavFile = SD_MMC.open("/myfile.wav", FILE_READ);
    if (!wavFile) {
      Serial.println("ERROR: failed to open /micTest.wav");
    } else {
      fileSize = wavFile.size();
      Serial.printf("Sending WAV file, size: %u bytes\n", fileSize);

      // sendRequest has an overload that takes a Stream*
      code = http.sendRequest("POST", &wavFile, fileSize);
      Serial.printf("[HTTP] POST code: %d\n", code);

      if (code > 0) {
        String response = http.getString();
        Serial.println("Response payload:");
        Serial.println(response);
      }
      wavFile.close();
    }
    http.end();  // always close the connection
  }
}

void sendConfig() {
  if (WiFi.status() == WL_CONNECTED) {
    String primary, secondary;
    if (!readCredentials(primary, secondary)) return;

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

    http.end();  // VERY important!
  } else {
    Serial.println("WiFi not connected.");
  }
}

void sendDataTask(void* pvParameters) {
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      String primary, secondary;
      if (!readCredentials(primary, secondary)) return;

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

      http.end();  // VERY important!
    } else {
      Serial.println("WiFi not connected.");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));  // for debugging, reduce interval
  }
}

void configWatcherTask(void* param) {
  while (1) {
    File f = SD_MMC.open("/samplingRateConfig.txt", FILE_READ);
    if (f) {
      uint32_t currentHash = calculateFileCRC32(f);
      if (currentHash != lastFileHash) {
        Serial.println("Detected change in samplingRateConfig.txt.");
        lastFileHash = currentHash;
        RestartDynamicRecordingTask();
      }
      f.close();
    } else {
      Serial.println("Failed to open samplingRateConfig.txt for monitoring.");
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 sec
  }
}

void RestartDynamicRecordingTask() {
  if (dynamicRecordingHandle) {
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
    0  // Core 0
  );

  Serial.println("Recording task restarted.");
}

void rtcDriverTask(void* param) {
  while (true) {
    PCF85063_Read_Time(&datetime);    // Update global datetime for UI
    vTaskDelay(pdMS_TO_TICKS(1000));  // Update every 1 sec
  }
}

void Driver_Init() {
  Flash_test();
  PWR_Init();
  BAT_Init();
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Backlight_Init();
  Set_Backlight(50);  //0~100
  PCF85063_Init();
  //QMI8658_Init();
}
void setup() {
  Serial.begin(115200);

  // Check PSRAM
  size_t ps = esp_psram_get_size();
  size_t sp = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf(">> PSRAM size: %u  SPIRAM free: %u\n", ps, sp);

  WiFi.begin(SSID, PASSWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
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

  SD_Init();
  //Audio_Init();
  //MIC_Init();
  LCD_Init();
  Lvgl_Init();

  ui_init();


  RestartDynamicRecordingTask();
  sendConfig();
  File f = SD.open("/samplingRateConfig.txt", FILE_READ);
  if (f) {
    lastFileHash = calculateFileCRC32(f);
    f.close();
  }

  xTaskCreatePinnedToCore(
    configWatcherTask,
    "ConfigWatcher",
    4096,
    NULL,
    1,
    NULL,
    0  // Core 0
  );
  xTaskCreatePinnedToCore(
    rtcDriverTask,
    "RTC Driver",
    2048,
    NULL,
    1,
    NULL,
    0  // Core 0
  );
}
void loop() {
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}
