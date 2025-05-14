# Smartwatch Project Development Notes
## Arduino IDE Related - How to setup Arduino IDE
To get started with Arduino IDE development, below are the steps:
  - [Download](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46/ESP32-S3-Touch-LCD-1.46-Demo.zip) ESP32-S3 Demo project from waveshare
  - Extract the LVGL_Arduino project from the zip to a folder as you wish
  - Open the file with .ino extension (That is in the folder that you just extracted). Arduino IDE will be opened.
  - Copy the following into the below specified location:
    -- Url to copy -> https://espressif.github.io/arduino-esp32/package_esp32_index.json
    -- On the top right corner of the IDE, open File->Preferences: ![alt](https://docs.espressif.com/projects/arduino-esp32/en/latest/_images/install_guide_preferences.png)
  - Open board manager (Left pane, second from top) and search for esp32. Install the second one (by Espressif Systems) with version 3.2.0
  -  Open Library manager and install below libraries:
> LVGL v8.3.10<br>
> ESP32-audioI2S-master v2.0.0
- Select the model of esp32 as it is in the waveshare document: Waveshare ESP32-S3-Touch.....
- Copy the contents of SquarelineOutput(I will push it with this README update commit) folder completely as they are, into your local LVGL_Arduino project
- Go to .ino file in your project folder and add "ui_init();" after Lvgl_Init(); function call.
- After that, connect esp32 to your computer and it will automatically find the board itself as there will appear only one port.
- Once the esp32 is recognized by the IDE, it should be okay to compile and flash. To do so, use the upload button (on the top left, there are 3 blue buttons. It is the one in the middle.). It may take ~5-7 minutes to compile and ~40 seconds to flash.
