#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"             // För xQueueSend / xQueueReceive
#include "gpio.h"
#include "ble_manager.h"
#include "alarm.h"
#include "disp_config.h"
#include "disp_ui.h"

//QueueHandle_t sensor_queue = NULL;
SemaphoreHandle_t xAlarmSemaphore;
SemaphoreHandle_t xGuiSemaphore; 
QueueHandle_t alarmQueue;

void app_main() {

    // skapa larm kö
    alarmQueue = xQueueCreate(10, sizeof(AlarmInfo));
    xAlarmSemaphore = xSemaphoreCreateBinary();
    xGuiSemaphore = xSemaphoreCreateMutex();
    //sensor_queue = xQueueCreate(5, sizeof(sensor_data_t)); // Om du har en sensor-kö

    disp_HW_init();
    disp_UI_init();
    initBLE();

    // Skapa RTOS tasks..
    xTaskCreatePinnedToCore(vBLETask, "BLE_HOST", 8192, NULL, 3, NULL, 0);          // CORE 0: För BLE task (ska ej ha 'TaskDelay')
    xTaskCreatePinnedToCore(vReceiveAlarmTask, "Rx_ALARM", 4096, NULL, 4, NULL, 0); // CORE 0: För 
    //xTaskCreatePinnedToCore(vWiFiTask, "WIFI_MQTT", 8192, NULL, 1, NULL, 0);      // CORE 0: För WiFi + MQTT
    xTaskCreatePinnedToCore(vAlarmManagerTask, "ALARM", 4096, NULL, 5, NULL, 1);      // CORE 1: För larmlogik (OBS: ej på H2, har bara en kärna)

    xTaskCreate(lvgl_port_task, "LVGL", 1024 * 16, NULL, 2, NULL);
    //xTaskCreate(sensor_read, "DHT11_Task", 1024 * 4, NULL, 2, NULL);
    
    vTaskDelete(NULL);
}
