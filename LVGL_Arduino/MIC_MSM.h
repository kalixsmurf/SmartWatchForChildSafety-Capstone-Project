#pragma once

#include "ESP_I2S.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include <ESP_SR.h>

#define I2S_PIN_BCK   15
#define I2S_PIN_WS    2
#define I2S_PIN_DOUT  -1
#define I2S_PIN_DIN   39

extern I2SClass i2s;

void MIC_Init(void);