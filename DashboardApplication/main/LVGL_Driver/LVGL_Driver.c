#include "LVGL_Driver.h"

static const char *TAG_LVGL = "LVGL";

lv_display_t *disp = NULL;
static lv_indev_t *indev = NULL;

void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// Optional: adjust memory alignment for DMA or 4-pixel-aligned drawing
void Lvgl_port_rounder_callback(lv_display_t * disp_drv, lv_area_t * area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    area->x1 = (x1 >> 2) << 2;
    area->x2 = ((x2 >> 2) << 2) + 3;
}

void example_lvgl_flush_cb(lv_display_t * disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp_drv);

    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              (lv_color_t *)px_map);

    lv_display_flush_ready(disp_drv);
}

void example_touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touchpad_x[5] = {0};
    uint16_t touchpad_y[5] = {0};
    uint8_t touchpad_cnt = 0;

    bool touchpad_pressed = Touch_Get_xy(touchpad_x, touchpad_y, NULL, &touchpad_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

lv_display_rotation_t example_lvgl_port_update_callback(lv_display_t * disp_drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp_drv);

    lv_display_rotation_t rotation = lv_display_get_rotation(disp_drv);

    switch (rotation) {
    case LV_DISPLAY_ROTATION_0:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    case LV_DISPLAY_ROTATION_90:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISPLAY_ROTATION_180:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISPLAY_ROTATION_270:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    default:
        break;
    }
    return rotation;
}

void LVGL_Init(void)
{
    ESP_LOGI(TAG_LVGL, "Initializing LVGL v9...");
    lv_init();

    // Allocate double buffers for display drawing
    lv_color_t *buf1 = heap_caps_malloc(LVGL_BUF_LEN * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(LVGL_BUF_LEN * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    assert(buf2);

    // Register display
    disp = lv_display_create(EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT);
    lv_display_set_buffers(disp, buf1, buf2, LVGL_BUF_LEN * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, example_lvgl_flush_cb);
    lv_display_set_user_data(disp, panel_handle);
    lv_disp_set_rotation(disp, example_lvgl_port_update_callback(disp));

    // Register input device
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, example_touchpad_read);
    lv_indev_set_disp(indev, disp);

    // Start LVGL tick using esp_timer
    ESP_LOGI(TAG_LVGL, "Starting LVGL tick timer...");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
}
