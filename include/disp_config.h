#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

// ========== PIN-CONFIG ========== -- S3 --
#define LCD_HOST       SPI2_HOST
#define PIN_MOSI       7  // SDI
#define PIN_MISO       13 // SDO
#define PIN_CLK        6 
#define PIN_CS         5  
#define PIN_DC         4 
#define PIN_RST        18 
#define PIN_TOUCH_CS   1
#define PIN_TOUCH_IRQ  2 

// Externa handtag som main/UI behöver nå
extern esp_lcd_touch_handle_t touch_handle;

void disp_HW_init();

#endif