#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "Display_SPD2010.h"  // Your display driver (should define panel_handle)
#include "esp_lcd_panel_io.h"

#define LVGL_BUF_LEN  (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT / 20)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

extern lv_display_t *disp; // LVGL v9 display handle

void example_lvgl_flush_cb(lv_display_t * disp_drv, const lv_area_t *area, uint8_t *px_map);
lv_display_rotation_t example_lvgl_port_update_callback(lv_display_t * disp_drv);
void example_increase_lvgl_tick(void *arg);
void LVGL_Init(void);  // Call this function to initialize LVGL and the display
