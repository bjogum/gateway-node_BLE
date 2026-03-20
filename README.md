isplay Test

This is a quick walkthrough of how to setup a quick test of an ILI9341 touch display with an ESP32-H2 to make sure everything is working correctly. This is a slimmed down version only focused on making sure that the display works properly, including touch.


## Initial setup, DO THIS BEFORE CLONING THIS REPO

First you need to make sure that PlatformIO recognizes the ESP32-H2 as a viable MCU, which probably requires a little work. 

When trying to make your first project in PIO you will have to select your MCU and framework, which in this case will be the ESP32-H2-DEVKITM-1 on the espidf framework. PlatformIO does not have support for that MCU initially, so you will have to add it manually. So you will first have to create a "dummy" project.

1. lnstall the PlatformIO extension inside of VSCode
2. When in the PIO Home-screen, select "New Project"
3. Select any ESP32 board, does not matter at all because we will change it after
4. Choose the espidf framework
5. Press "Finish" and let PIO configure the project, this might take a few minutes
6. When completed, go into the platformio.ini file and change it to look like this:
```
[env:esp32-h2-devkitm-1]
platform = https://github.com/Jason2866/platform-espressif32.git#Arduino/IDF53
board  = esp32-h2-devkitm-1
framework = espidf
```
 
PlatformIO should now automatically configure everything add the esp32-h2-devkitm-1 to the list of available boards.

These steps are only required to do once, the h2 will be added permanently to this config of PIO.

## Wiring
Will require breadboard because some of of the pins share GPIO.
```
Display:                ESP32:
    VCC + LED/BL            3.3v
    GND                     GND
    MOSI/SDI + T_DIN        25
    MISO + T_OUT            11
    CLK/SCK + T_CLK         10
    CS                      0
    DC                      1
    RST/RESET               4
    T_CS                    2
    T_IRQ                   5
```
