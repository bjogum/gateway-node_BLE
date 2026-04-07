#include <stdio.h>
#include "disp_config.h"  
#include "disp_ui.h"    
#include "gpio.h"
#include "alarm.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"  
#include "ble_manager.h"  

lv_obj_t * inside_box = NULL;
lv_obj_t * inside_temp  = NULL;
lv_obj_t * inside_humid = NULL;
static lv_obj_t * alarm_btn = NULL;
static lv_obj_t * alarm_label = NULL;

// Menu-prototyopes
lv_obj_t * ui_home_screen_init(void);
lv_obj_t * ui_options_screen_init(void);
lv_obj_t * ui_status_screen_init(void);

static void clear_home_screen_pointers(){
    inside_box = NULL;
    inside_temp = NULL;
    inside_humid = NULL;
    alarm_btn = NULL;
    alarm_label = NULL;
}

static void change_to_options_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Create options and remove home_screen (true = auto delete)
        clear_home_screen_pointers();
        lv_obj_t * next_scr = ui_options_screen_init(); 
        lv_screen_load_anim(next_scr, LV_SCR_LOAD_ANIM_FADE_ON, 10, 0, true);
    }
}
static void change_to_home_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        clear_home_screen_pointers();
        
        lv_obj_t * next_scr = ui_home_screen_init();
        lv_screen_load_anim(next_scr, LV_SCR_LOAD_ANIM_FADE_ON, 10, 0, true);
    }
} 
static void change_to_status_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        clear_home_screen_pointers();
        
        lv_obj_t * next_scr = ui_status_screen_init();
        lv_screen_load_anim(next_scr, LV_SCR_LOAD_ANIM_FADE_ON, 10, 0, true);
    }
}
    
static void update_alarm_ui(void) {
    if (!alarm_btn || !alarm_label) return;
    switch (node.systemState) {
    case STATE_DISARMED:
        lv_label_set_text(alarm_label, "DISARMED");
        lv_obj_set_style_bg_color(alarm_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_set_style_bg_color(inside_box, lv_palette_main(LV_PALETTE_GREEN), 0);
        break;
    case STATE_ARMED_AWAY:
        lv_label_set_text(alarm_label, "ARMED_AWAY");
        lv_obj_set_style_bg_color(alarm_btn, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_bg_color(inside_box, lv_palette_main(LV_PALETTE_RED), 0);
        break;
    case STATE_ARMED_HOME: //to be triggered by sensors, uncomment when posible
        //lv_label_set_text(alarm_label, "TRIGGERED");
        //lv_obj_set_style_bg_color(alarm_btn, lv_palette_main(LV_PALETTE_RED), 0); 
        break;
    }
}

static void alarm_toggle_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    setAlarmState(STATE_ARMED_AWAY);                    // <<------------ !! JUST FOR TEST !! <<------------
    printf("Alarm state changed to: %d\n", node.systemState);
    //node.alarmState = (alarm_state + 1) % 2; //  change  to "% 3" when correct sensors are added <------ Behöver uppdateras!
    update_alarm_ui();
}
// ====== HOMESCREEN ========
    lv_obj_t * ui_home_screen_init(void) {
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
        return scr;
    }
    // ======== Settings =======
    lv_obj_t * ui_options_screen_init(void){
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
        return scr;
    }
    // ====== Status ======
    lv_obj_t * ui_status_screen_init(void){
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
        return scr;
    }
void lvgl_port_task(void *arg){
    ui_home_screen_init();
    bl_pwm_init();
    
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        lv_obj_t * main_scr = ui_home_screen_init();
        lv_screen_load(main_scr);
        xSemaphoreGive(xGuiSemaphore);
    }
    
    //sensor_data_t received_msg;

    while (1) {
        uint32_t time_till_next = 10;
        if (xSemaphoreTake(xGuiSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            //if (xQueueReceive(sensor_queue, &received_msg, 0) == pdTRUE) {
            //    if (inside_temp != NULL && inside_humid != NULL) {
            //        lv_label_set_text_fmt(inside_temp, "%d°C", (int)received_msg.temperature);
            //        lv_label_set_text_fmt(inside_humid, "%d %%", (int)received_msg.humidity);
            //    }
            //}
            uint32_t idle_time = lv_display_get_inactive_time(NULL); // Returns ms
            if (idle_time > 5000) { // 5s of inactivity
                set_bl_brightness(1); // Dim to 5%
            } else {
                set_bl_brightness(50); // 50% as standard
            }
            time_till_next = lv_timer_handler();
            
            xSemaphoreGive(xGuiSemaphore);
        }

        lv_tick_inc(10); 
        if (time_till_next < 10) time_till_next = 10;
        if (time_till_next > 50) time_till_next = 50;
        
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

void disp_UI_init(void) {
    lv_obj_t * main_scr = ui_home_screen_init();
    lv_screen_load(main_scr);
}