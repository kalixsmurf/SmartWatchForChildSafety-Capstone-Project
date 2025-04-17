// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.1
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#include "../ui.h"

void ui_SaveSamplingRateNotification_screen_init(void)
{
    ui_SaveSamplingRateNotification = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SaveSamplingRateNotification, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Label5 = lv_label_create(ui_SaveSamplingRateNotification);
    lv_obj_set_width(ui_Label5, 350);
    lv_obj_set_height(ui_Label5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label5, 0);
    lv_obj_set_y(ui_Label5, -120);
    lv_obj_set_align(ui_Label5, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_Label5, "Sampling Rate Selection Saved");
    lv_obj_set_style_text_font(ui_Label5, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel2 = lv_obj_create(ui_SaveSamplingRateNotification);
    lv_obj_set_width(ui_Panel2, 140);
    lv_obj_set_height(ui_Panel2, 140);
    lv_obj_set_x(ui_Panel2, 0);
    lv_obj_set_y(ui_Panel2, -50);
    lv_obj_set_align(ui_Panel2, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_img_src(ui_Panel2, &ui_img_checked_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Panel2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_SaveSamplingRateNotification, ui_event_SaveSamplingRateNotification, LV_EVENT_ALL, NULL);

}
