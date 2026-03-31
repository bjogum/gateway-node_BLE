#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ble_manager.h" 
#include "alarm.h"
#include "gpio.h"

AlarmInfo alarmInfo =  {NONE, 0};

System node = {
    .runStatus = WAKING_UP,
    .connectionStatus = {
        .wifiIsActive = false,
        .bleIsActive = false,
        .mqttIsActive = false,
    },
    .systemState = STATE_DISARMED,
    .alarmStatus = {
        .ALARM_FIRE = false,
        .ALARM_INTRUSION = false,
        .ALARM_WATER = false,
    },
    .sensorData = {
        .indoorHumidity = 0.0,
        .indoorTemp = 0.0,
        .wether = {
            .API_description = {0},
            .API_humid = 0.0,
            .API_temp = 0.0,
        },
    },
    .sysTime = 0,
};


void vAlarmReceiveTask(void* params){
    // lokal "behållare" för värdet som tas emot från kön.
    buzzer_init();
    
    for (;;){
        // vänta här - tills något finns i kön.. -> (kopigera då in värdet i 'val')
        if (xQueueReceive(alarmQueue, &alarmInfo, portMAX_DELAY)){
            ESP_LOGI("SensorNode", "Data: %d", alarmInfo.trigger);
            ESP_LOGI("SensorNode", "Tid: %d", alarmInfo.time);

            if (alarmInfo.trigger != NONE) {
                manageAlarm(); 
            }
        }
        //vTaskDelay ..
    }
}


//void alarmSendState(){
    // skickar till arudino: 0,1,2 (STATE_DISARMED, STATE_ARMED_AWAY, STATE_ARMED_HOME)

//}

void manageAlarm(){
    // bestäm hur vi ska aggera, beroende på larm.

    // sätter larm-status
    setAlarm();

    // agerar på larm-status
    if (node.alarmStatus.ALARM_FIRE){       // hög prio
        buzzer_on_fire();
        // Visuellt: Tydligt i display
        // Ljud: högt-tätt pip ?

        // SKICKA (Tx):
        // broker -> DB
        // thingsboard ?

        // RESET:
        // inaktiveras ~8s efter larm inkommer (larm kommer löpande var 5-6s tills temp är OK)
        // via resetAlarm(); 
    }

    if (node.alarmStatus.ALARM_INTRUSION){  // mellan prio
        buzzer_on_intrusion();
        // Visuellt: Tydligt i display
        // Ljud: hög siren ?

        // SKICKA (Tx):
        // broker -> DB
        // thingsboard ?

        // RESET:
        // inaktiveras när larm inaktiveras av användare - via resetAlarm();
    }

        if (node.alarmStatus.ALARM_WATER){  // låg prio
        // Visuellt: Info i display
        // Ljud: Lätt pip

        // SKICKA (Tx):
        // broker -> DB
        // thingsboard ?

        // RESET:
        // inaktiveras ~1 min efter larm slutat skicka ?
    }
}


void setAlarm(){
    // om vi fått larm, sätt status
    switch (alarmInfo.trigger)
    {
    case FIRE:
        node.alarmStatus.ALARM_FIRE = true;
        break;
    case DOOR:
        node.alarmStatus.ALARM_INTRUSION = true;
    break;
    case MOTION:
        node.alarmStatus.ALARM_INTRUSION = true;
    break;
    case WATER:
        node.alarmStatus.ALARM_WATER = true;
        // Infomation on LCD / Thingsboard + bip.
    break;
    default:
        // failstate?
        break;
    }

}

//void fireAlarm(){
    // start fire alarm 
    // remain ~8s (Rx each ~6s cycles)
    //buzzer + LCD/Thingsboard??

    
    //buzzer_on();
    
    // else
    //buzzer_off();
//}