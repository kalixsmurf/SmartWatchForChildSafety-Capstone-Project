#include "Wireless.h"


bool WIFI_Connection = 0;
uint8_t WIFI_NUM = 0;
uint8_t BLE_NUM = 0;
bool Scan_finish = 0;
int wifi_scan_number()
{
  printf("/**********WiFi Test**********/\r\n");
  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
  WiFi.mode(WIFI_STA);                           
  WiFi.setSleep(true);     
  // WiFi.scanNetworks will return the number of networks found.
  int count = WiFi.scanNetworks();
  if (count == 0)
  {
    printf("No WIFI device was scanned\r\n");
  }
  else{
    printf("Scanned %d Wi-Fi devices\r\n",count);
  }
  
  
  WiFi.disconnect(true);
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF); 
  vTaskDelay(100);            
  printf("/*******WiFi Test Over********/\r\n\r\n");
  return count;
}
int ble_scan_number()
{
  printf("/**********BLE Test**********/\r\n"); 
  BLEDevice::init("ESP32");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  BLEScanResults* foundDevices = pBLEScan->start(5);   
  int count = foundDevices->getCount();
  if (count == 0)
  {
    printf("No Bluetooth device was scanned\r\n");
  }
  else{
    printf("Scanned %d Bluetooth devices\r\n",count);
  }
  pBLEScan->stop(); 
  pBLEScan->clearResults(); 
  BLEDevice::deinit(true); 
  vTaskDelay(100);           
  printf("/**********BLE Test Over**********/\r\n\r\n");
  return count;
}
extern char buffer[128];    /* Make sure buffer is enough for `sprintf` */
void Wireless_Test1(){
  // BLE_NUM = ble_scan_number();                       // !!! Please note that continuing to use Bluetooth will result in allocation failure due to RAM usage, so pay attention to RAM usage when Bluetooth is turned on
  WIFI_NUM = wifi_scan_number();
  Scan_finish = 1;
}
void WirelessScanTask(void *parameter) {
  // BLE_NUM = ble_scan_number();                       // !!! Please note that continuing to use Bluetooth will result in allocation failure due to RAM usage, so pay attention to RAM usage when Bluetooth is turned on
  WIFI_NUM = wifi_scan_number();
  Scan_finish = 1;
  vTaskDelay(pdMS_TO_TICKS(1000));
  vTaskDelete(NULL);
}
void Wireless_Test2(){
  xTaskCreatePinnedToCore(
    WirelessScanTask,    
    "WirelessScanTask",   
    4096,              
    NULL,                
    2,                  
    NULL,                 
    0                    
  );
}

bool ConnectWifi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  printf("Connecting to Wi-Fi: %s\n", ssid);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    printf(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    printf("\nFailed to connect to Wi-Fi\n");
    return false;
  }

  printf("\nConnected to Wi-Fi. IP address: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool SendData(const char* url, const char* jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    printf("Wi-Fi not connected!\n");
    return false;
  }

  HTTPClient http;
  http.begin(url);  // e.g., "http://192.168.1.100:5000/api/data"
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    printf("HTTP Response code: %d\n", httpResponseCode);
    printf("Response: %s\n", response.c_str());
  } else {
    printf("Error in sending POST request: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  return httpResponseCode > 0;
}
