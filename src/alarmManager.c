#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "bleManager.h" 
#include "alarmManager.h"

AlarmInfo alarmInfo =  {NONE, 0};

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