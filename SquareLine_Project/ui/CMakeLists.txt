SET(SOURCES screens/ui_LoginScreen.c
    screens/ui_MainPage.c
    screens/ui_FilteringOptionsPage.c
    screens/ui_PredictionsPage.c
    screens/ui_ProfilePage.c
    screens/ui_LockPage.c
    screens/ui_SaveScreen.c
    screens/ui_FilteringOptionsPage2.c
    screens/ui_CurrentSamplingRateSelections.c
    screens/ui_SaveSamplingRateNotification.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_padlock_png.c
    images/ui_img_filter_png.c
    images/ui_img_analytics_png.c
    images/ui_img_user_png.c
    images/ui_img_checked_png.c
    images/ui_img_plus_png.c)

add_library(ui ${SOURCES})
