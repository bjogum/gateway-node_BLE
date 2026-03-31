#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ble_manager.h" 
#include "alarm.h"

AlarmInfo alarmInfo =  {NONE, 0};

System node = {
    .runStatus = WAKING_UP,
    .connectionStatus = {
    .wifiIsActive = false,
    .bleIsActive = false,
    .mqttIsActive = false,
  },
    .alarmState = STATE_DISARMED,
    .alarmStatus = IDLE,
    .sensorData = {
    .indoorHumidity = 0.0,
    .indoorTemp = 0.0,
    .openWether = {
        .API_description = {0},
        .API_humid = 0.0,
        .API_temp = 0.0,
        },
    },
    .sysTime = 0,
};


void vAlarmReceiveTask(void* params){
    // lokal "behållare" för värdet som tas emot från kön.

    for (;;){
        // vänta här - tills något finns i kön.. -> (kopigera då in värdet i 'val')
        if (xQueueReceive(alarmQueue, &alarmInfo, portMAX_DELAY)){
            ESP_LOGI("SensorNode", "Data: %d", alarmInfo.trigger);
            ESP_LOGI("SensorNode", "Tid: %d", alarmInfo.time);
        }
    }
}


//void alarmSendState(){
    // skickar till arudino: 0,1,2 (disarmed, armed-away, armed-home)

//}