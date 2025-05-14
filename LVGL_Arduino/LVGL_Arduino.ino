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
#include "PCF85063.h"
#include "QMI8658.h"
#include "SD_MMC.h"
#include "Wireless.h"
#include "TCA9554PWR.h"
#include "PCM5101.h"
#include "lvgl.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

extern "C" {
  #include "Record_Audio.c"
}

#define SD_SPI_HOST SPI2_HOST
#define SD_SPI_MOSI GPIO_NUM_11
#define SD_SPI_MISO GPIO_NUM_13
#define SD_SPI_SCK  GPIO_NUM_12
#define SD_SPI_CS   GPIO_NUM_10
#define MOUNT_POINT "/sdcard"

void Driver_Loop(void *parameter)
{
  Wireless_Test2();
  while (1)
  {
    PWR_Loop();
    QMI8658_Loop();
    PCF85063_Loop();
    BAT_Get_Volts();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
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
  QMI8658_Init();
}
void Audio_Task(void *param)
{
  start_recording(); // Must be non-blocking or handle vTaskDelay internally
  vTaskDelete(NULL);
}
void setup()
{
  Driver_Init();

  SD_Init();
  Audio_Init();
  MIC_Init();
  LCD_Init();
  Lvgl_Init();
  ui_init();
  
  xTaskCreatePinnedToCore(
      Driver_Loop,
      "Other Driver task",
      2048,
      NULL,
      3,
      NULL,
      0);
  // sd_card_initialization();
  xTaskCreatePinnedToCore(
      Audio_Task,
      "Audio Recording",
      4096,
      NULL,
      3,
      NULL,
      1 // Use Core 1 if UI is on Core 0
  );
}
void loop()
{
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}
