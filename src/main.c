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
#include "dht11_driver.h"
#include "led_pwm.h"

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
static bool hardware_ready = false;
lv_obj_t * inside_box = NULL;
lv_obj_t * inside_temp  = NULL;
lv_obj_t * inside_humid = NULL;

// ======== ALARM STATE ========
typedef enum {
    ALARM_OFF = 0,
    ALARM_ARMED,
    ALARM_TRIGGERED,
} alarm_state_t;

static alarm_state_t alarm_state = ALARM_OFF;
static lv_obj_t * alarm_btn = NULL;
static lv_obj_t * alarm_label = NULL;

// ========== SENSOR READING ==========
void sensor_read(void *pvParameters){
    float temp = 0;
    float hum = 0;
    int status = 0;

    while(1){
        status = read_dht11(GPIO_NUM_8, &temp, &hum);
        
        if(status == 0){
            if(inside_temp != NULL && inside_humid != NULL){
                int t_int = (int)temp;
                int h_int = (int)hum;
                lv_label_set_text_fmt(inside_temp, "%d°C", t_int);
                lv_label_set_text_fmt(inside_humid, "%d %%", h_int);
                printf("Temp: %.1f C, Hum: %.0f%%\n", temp, hum);
            } 
        } else {printf("Sensor failed, error code: %d\n", status);}
        vTaskDelay(pdMS_TO_TICKS(10000));
    } 
}

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

// Menu-prototyopes
void ui_home_screen_init(void);
void ui_options_screen_init(void);
void ui_status_screen_init(void);

static void change_to_options_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Create options and remove home_screen (true = auto delete)
        ui_options_screen_init();
        lv_screen_load_anim(lv_screen_active(), LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    }
}

static void change_to_home_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_home_screen_init();
        lv_screen_load_anim(lv_screen_active(), LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    }
} 

static void change_to_status_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_status_screen_init();
        lv_screen_load_anim(lv_screen_active(), LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
    }
}

static void update_alarm_ui(void) {
    if (!alarm_btn || !alarm_label) return;
    switch (alarm_state) {
    case ALARM_OFF:
        lv_label_set_text(alarm_label, "Alarm OFF");
        lv_obj_set_style_bg_color(alarm_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_set_style_bg_color(inside_box, lv_palette_main(LV_PALETTE_GREEN), 0);
        break;
    case ALARM_ARMED:
        lv_label_set_text(alarm_label, "ARMED");
        lv_obj_set_style_bg_color(alarm_btn, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_bg_color(inside_box, lv_palette_main(LV_PALETTE_RED), 0);
        break;
    /* case ALARM_TRIGGERED: //to be triggered by sensors, uncomment when posible
        //lv_label_set_text(alarm_label, "TRIGGERED");
        //lv_obj_set_style_bg_color(alarm_btn, lv_palette_main(LV_PALETTE_RED), 0); 
        break; */
    }
}

static void alarm_toggle_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    printf("Alarm state changed to: %d\n", alarm_state);
    alarm_state = (alarm_state + 1) % 2; //  change  to "% 3" when correct sensors are added
    update_alarm_ui();
}

// ====== HOMESCREEN ========
void ui_home_screen_init(void) {
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); //black

    // Graphics Container
    lv_obj_t * container = lv_obj_create(scr);
    lv_obj_set_size(container, 280, 180);
    lv_obj_align(container, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(container,  0, 0);
    lv_obj_set_style_border_width(container,  0, 0);
    
    // Roof
    static lv_point_precise_t roof_points[] = {{0,60}, {70,0}, {140,60}};
    lv_obj_t * roof = lv_line_create(container);
    lv_line_set_points(roof, roof_points, 3);
    lv_obj_set_style_line_width(roof, 3, 0);
    lv_obj_set_style_line_color(roof, lv_color_white(), 0);
    lv_obj_align(roof, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Inside Box
    inside_box = lv_obj_create(container);
    lv_obj_set_size(inside_box, 140, 100);
    lv_obj_align(inside_box, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(inside_box, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_radius(inside_box, 0, 0);
    lv_obj_set_style_border_width(inside_box, 2, 0);
    lv_obj_set_style_border_color(inside_box, lv_color_white(), 0);
    lv_obj_set_style_radius(inside_box, 20, 0);

    // Outside Box
    lv_obj_t * outside_box = lv_obj_create(container);
    lv_obj_set_size(outside_box, 110, 100);
    lv_obj_align(outside_box, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(outside_box, 0, 0); // Transparent
    lv_obj_set_style_border_width(outside_box, 2, 0);
    lv_obj_set_style_border_color(outside_box, lv_color_white(), 0);
    lv_obj_set_style_border_side(outside_box, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_radius(outside_box, 20, 0);

    // labels
        // inside
    lv_obj_t * inside_header = lv_label_create(inside_box);
    lv_label_set_text(inside_header, "Inside");
    lv_obj_set_style_text_color(inside_header, lv_color_white(), 0);
    lv_obj_align(inside_header, LV_ALIGN_TOP_MID, 0, 0);

    inside_temp = lv_label_create(inside_box);
    lv_obj_set_style_text_font(inside_temp, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(inside_temp, lv_color_white(), 0);
    lv_obj_align(inside_temp, LV_ALIGN_CENTER, 0, 0);

    inside_humid = lv_label_create(inside_box);
    lv_obj_set_style_text_font(inside_humid, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(inside_humid, lv_color_white(), 0);
    lv_obj_align(inside_humid, LV_ALIGN_CENTER, 0, 20);

        // outside
    lv_obj_t * outside_header = lv_label_create(outside_box);
    lv_label_set_text(outside_header, "Outside");
    lv_obj_set_style_text_color(outside_header, lv_color_white(), 0);
    lv_obj_align(outside_header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * outside_temp = lv_label_create(outside_box);
    lv_label_set_text(outside_temp, "-4°C");
    lv_obj_set_style_text_font(outside_temp, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(outside_temp, lv_color_white(), 0);
    lv_obj_align(outside_temp, LV_ALIGN_CENTER, 0, 10);

    // buttons
    // Alarm toggle
    alarm_btn = lv_button_create(scr);
    lv_obj_set_size(alarm_btn, 80, 25);
    lv_obj_align(alarm_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_border_width(alarm_btn, 2, 0);
    lv_obj_set_style_border_color(alarm_btn, lv_color_white(), 0);

    // Register callback
    lv_obj_add_event_cb(alarm_btn, alarm_toggle_cb, LV_EVENT_CLICKED, NULL);
    alarm_label = lv_label_create(alarm_btn);
    lv_obj_align(alarm_label, LV_ALIGN_CENTER, 0, 0);
    update_alarm_ui();
    
    // Options
    lv_obj_t * opt_btn = lv_button_create(scr);
    lv_obj_set_size(opt_btn, 80, 25);
    lv_obj_align(opt_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(opt_btn, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t * label = lv_label_create(opt_btn);
    lv_label_set_text(label, "Options");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Status
    lv_obj_t * stat_btn = lv_button_create(scr);
    lv_obj_set_size(stat_btn, 80, 25);
    lv_obj_align(stat_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_bg_color(stat_btn, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t * stat_label = lv_label_create(stat_btn);
    lv_label_set_text(stat_label, "Status");
    lv_obj_align(stat_label, LV_ALIGN_CENTER, 0, 0);

    // Register callback
    lv_obj_add_event_cb(opt_btn, change_to_options_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(stat_btn, change_to_status_cb, LV_EVENT_CLICKED, NULL);

    lv_screen_load(scr);
}

// ======== Settings =======
void ui_options_screen_init(void){
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); //black

    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 80, 25);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Back");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Register callback
    lv_obj_add_event_cb(btn, change_to_home_cb, LV_EVENT_CLICKED, NULL);
    lv_screen_load(scr);
}

// ====== Status ======
void ui_status_screen_init(void){
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); //black

    lv_obj_t * header = lv_label_create(scr);
    lv_label_set_text(header, "SENSOR STATUS");
    lv_obj_set_style_text_color(header, lv_color_white(), 0);
    lv_obj_align(header,LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t * stat_list = lv_obj_create(scr);
    lv_obj_set_size(stat_list, 280,  160);
    lv_obj_align(stat_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(stat_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(stat_list, 0, 0);
    lv_obj_set_style_pad_gap(stat_list, 10, 0);

    lv_obj_t  * row1 = lv_label_create(stat_list);
    lv_label_set_text(row1, "Temp+Hum (inside): ONLINE");
    lv_obj_set_style_text_color(row1,  lv_color_white(), 0);

    lv_obj_t  * row2 = lv_label_create(stat_list);
    lv_label_set_text(row2, "Door (Main): ONLINE");
    lv_obj_set_style_text_color(row2,  lv_color_white(), 0);

    lv_obj_t  * row3 = lv_label_create(stat_list);
    lv_label_set_text(row3, "Window (LR): ONLINE");
    lv_obj_set_style_text_color(row3,  lv_color_white(), 0);
    
    lv_obj_t  * row4 = lv_label_create(stat_list);
    lv_label_set_text(row4, "Leakage (Kitchen): ONLINE");
    lv_obj_set_style_text_color(row4,  lv_color_white(), 0);

    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 80, 25);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Back");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Register callback
    lv_obj_add_event_cb(btn, change_to_home_cb, LV_EVENT_CLICKED, NULL);
    lv_screen_load(scr);
}

static void lvgl_port_task(void *arg) {
    // Wait here until app_main says it's okay to start
    ui_home_screen_init(); 
    bl_pwm_init();
    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        lv_tick_inc(10); 
        
        uint32_t idle_time = lv_display_get_inactive_time(NULL); // Returns ms
        if (idle_time > 5000) { // 5s of inactivity
            set_bl_brightness(1); // Dim to 5%
        } else {
            set_bl_brightness(50); // 50% as standard
        }

        if (time_till_next < 1) time_till_next = 1;
        if (time_till_next > 50) time_till_next = 50;
        
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

void app_main() {
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
    
    xTaskCreate(lvgl_port_task, "LVGL", 1024 * 8, NULL, 5, NULL);

    xTaskCreate(sensor_read, "DHT11_Task", 1024 * 4, NULL, 2, NULL);
    
    vTaskDelete(NULL);
}