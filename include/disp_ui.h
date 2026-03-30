#ifndef LCD_UI_H
#define LCD_UI_H
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

extern lv_obj_t * inside_temp;
extern lv_obj_t * inside_humid;
extern SemaphoreHandle_t xGuiSemaphore;
extern QueueHandle_t sensor_queue;

// 2. Funktioner som main ska anropa
void disp_UI_init(void);
void lvgl_port_task(void *arg);

// TA ev bort sen.... för DHT11
typedef struct {
    float temperature;  
    float humidity;
} sensor_data_t;

#endif