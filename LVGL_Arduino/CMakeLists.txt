SET(SOURCES ui_LoginScreen.c
    ui_MainPage.c
    ui_FilteringOptionsPage.c
    ui_PredictionsPage.c
    ui_ProfilePage.c
    ui_LockPage.c
    ui_SaveScreen.c
    ui_FilteringOptionsPage2.c
    ui_CurrentSamplingRateSelections.c
    ui_SaveSamplingRateNotification.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.cpp
    ui_img_padlock_png.c
    ui_img_filter_png.c
    ui_img_analytics_png.c
    ui_img_user_png.c
    ui_img_checked_png.c
    ui_img_plus_png.c)

add_library(ui ${SOURCES})
