#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_xpt2046.h"

// ========== PIN-CONFIG ==========
#define LCD_HOST       SPI2_HOST
#define PIN_MOSI       25
#define PIN_MISO       11
#define PIN_CLK        10
#define PIN_CS         0
#define PIN_DC         1
#define PIN_RST        4
#define PIN_TOUCH_CS   2
#define PIN_TOUCH_IRQ  5


// ========== GLOBALS ==========

esp_lcd_touch_handle_t touch_handle = NULL;
static lv_display_t * disp_global = NULL;


// ========== TOUCH CALLBACKS ==========

    //lvgl touch callback
static void lvgl_touch_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    esp_lcd_touch_point_data_t point;
    uint8_t touch_cnt = 0;
    esp_lcd_touch_read_data(touch_handle); 
    esp_err_t err = esp_lcd_touch_get_data(touch_handle, &point, &touch_cnt, 1);
    if (err == ESP_OK && touch_cnt > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
        //printf("X: %d, Y: %d\n", point.x, point.y); //remove comment to log coordinates in serial monitor
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

    //callback for button
static void btn_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    if(lv_event_get_code(e) == LV_EVENT_PRESSED) {
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
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

// Menu-prototyopes
void ui_home_screen_init(void);
void ui_settings_screen_init(void);

// ======== SHARED CALLBACKS =========

static void change_to_settings_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Create settings and remove home_screen (true = auto_del)
        ui_settings_screen_init();
        lv_screen_load_anim(lv_screen_active(), LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    }
}

static void change_to_home_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Create home_screen and remove settings (true = auto_del)
        ui_home_screen_init();
        lv_screen_load_anim(lv_screen_active(), LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    }
}

// ====== HOMESCREEN ========
void ui_home_screen_init(void) {
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Settings");

    // Register callback
    lv_obj_add_event_cb(btn, change_to_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_screen_load(scr);
}

// ======== Settings =======
void ui_settings_screen_init(void) {
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Back");

    // Register callback
    lv_obj_add_event_cb(btn, change_to_home_cb, LV_EVENT_CLICKED, NULL);

    lv_screen_load(scr);
}

void app_main() {
    // ===== Initiate SPI-bus =====
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_CLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // ===== LVGL Core =====
    lv_init();
    disp_global = lv_display_create(240, 320);

    // ===== LCD-panel IO and driver =====
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = 20 * 1000 * 1000, // pixel clock frequency; Higher=faster updates but might introduce instability
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
    esp_lcd_panel_disp_on_off(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);


    // ===== LVGL Display settings =====
    lv_display_set_user_data(disp_global, panel_handle);
    lv_display_set_flush_cb(disp_global, lvgl_flush_cb);
    lv_display_set_color_format(disp_global, LV_COLOR_FORMAT_RGB565);

    // Allokera DMA-minne för buffert
    void *buf1 = heap_caps_malloc(240 * 80 * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_display_set_buffers(disp_global, buf1, NULL, 240 * 40 * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Skapa en IO-konfiguration för touch (ny CS-pin)
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(PIN_TOUCH_CS);
    tp_io_config.pclk_hz = 1 * 1000 * 1000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &tp_io_config, &tp_io_handle));

    // Skapa touch-instansen
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 240,
        .y_max = 320,
        .rst_gpio_num = -1, // Ofta inte ansluten
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &touch_handle));

    // ======== LVGL ==========
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    // Create UI
    // lv_obj_t *scr = lv_screen_active();
    // lv_obj_set_style_bg_color(scr, lv_palette_main(LV_PALETTE_RED), 0);

    // lv_obj_t * btn = lv_button_create(scr);
    // lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);
    // lv_obj_set_size(btn, 100, 40);
    // lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);

    // lv_obj_t * label = lv_label_create(btn);
    // lv_label_set_text_static(label, "Click");
    // lv_obj_center(label);

    ui_home_screen_init(); // Starta första skärmen

    while (1) {
        // 1. Hantera timers och få tid till nästa körning
        uint32_t time_till_next = lv_timer_handler();
        
        // 2. Öka LVGL-ticks (vi mäter tiden som faktiskt gått)
        lv_tick_inc(10); 

        // 3. Strömspar: Sov exakt så länge som behövs (max 50ms för responsivitet)
        if (time_till_next > 50) time_till_next = 50;
        if (time_till_next < 1) time_till_next = 1;
        
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}