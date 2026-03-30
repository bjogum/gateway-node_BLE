#include "disp_config.h"      
#include "lvgl.h"
#include "disp_ui.h" // Din egen header
#include <stdio.h>
#include "esp_lcd_ili9341.h" 
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_xpt2046.h"  // Touch-drivrutin


static lv_display_t * disp_global = NULL;
esp_lcd_touch_handle_t touch_handle = NULL;
static bool hardware_ready = false;


    // ========== TOUCH CALLBACKS ==========

        //lvgl touch callback
static void lvgl_touch_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    esp_lcd_touch_point_data_t point;
    uint8_t touch_cnt = 0;
    esp_lcd_touch_read_data(touch_handle); 
    esp_err_t err = esp_lcd_touch_get_data(touch_handle, &point, &touch_cnt, 1);
    if (err == ESP_OK && touch_cnt > 0) {
        data->point.x = point.x; 
        //data->point.y = point.y; //use this instead if up/down touch is mirrored
        data->point.y = 240- point.y; 
        data->state = LV_INDEV_STATE_PRESSED;
        printf("X: %d, Y: %d\n", point.x, point.y); //remove comment to log coordinates in serial monitor
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
    // Callback for when the hardware is done with SPI-transfer
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    if (disp_global) {
        lv_display_flush_ready(disp_global);
    }
    return false;
}
    // Flush called on by LVGL to "paint" an area
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lv_draw_sw_rgb565_swap(px_map, w * h); // Switch bytes (Little -> Big Endian)
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map); // Sends pixels to ILI9341
}

void disp_HW_init(){

    // ===== Display Constants =====
    const uint16_t LCD_H_RES = 320;
    const uint16_t LCD_V_RES = 240;
    const uint16_t BUF_LINES = 20; 
    const uint32_t BUF_SIZE_PX = LCD_H_RES * BUF_LINES; 
    const uint32_t BUF_SIZE_BYTES = LCD_H_RES * BUF_LINES * sizeof(uint16_t);

    // ===== Initiate SPI-bus =====
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_CLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // Match the max transfer size precisely to our buffer
        .max_transfer_sz = BUF_SIZE_BYTES, 
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));


    // ===== LVGL Core =====
    lv_init();
    disp_global = lv_display_create(LCD_H_RES, LCD_V_RES);

    // ===== LCD-panel IO and driver =====
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = 20 * 1000 * 1000, 
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = disp_global, 
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

    esp_lcd_panel_swap_xy(panel_handle, true);
    // If colors or directions are inverted, toggle true/false here
    esp_lcd_panel_mirror(panel_handle, false, false); 

    esp_lcd_panel_disp_on_off(panel_handle, true);

    // ===== LVGL Display settings =====
    lv_display_set_user_data(disp_global, panel_handle);
    lv_display_set_flush_cb(disp_global, lvgl_flush_cb);
    lv_display_set_color_format(disp_global, LV_COLOR_FORMAT_RGB565);
    // Allocate DMA buffer
    void *buf1 = heap_caps_malloc(BUF_SIZE_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (buf1 == NULL) {
        buf1 = heap_caps_malloc(LCD_H_RES * 10 * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    }

    lv_display_set_buffers(disp_global, buf1, NULL, BUF_SIZE_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // ===== Touch IO and Driver =====
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(PIN_TOUCH_CS);
    tp_io_config.pclk_hz = 1 * 1000 * 1000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &tp_io_config, &tp_io_handle));
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 240,
        .y_max = 320,
        .rst_gpio_num = -1, 
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,  // Swap touch axis to match display
            .mirror_x = 0, // Toggle to 1 if left/right is backwards
            .mirror_y = 1, // Toggle to 0 if up/down is backwards
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &touch_handle));

    // ======== LVGL Touch Setup ==========
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    hardware_ready = true;

}