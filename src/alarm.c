#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ble_manager.h" 
#include "alarm.h"
#include "gpio.h"
#define FIRE_ALARM_RESET_TIME 8000
#define WATER_ALARM_RESET_TIME 35000
#define HEARTBEAT_BLE_INTERVALL 5000

int lastHeartbeatTime = -1;
int triggerFireAlarmTime;
int triggerWaterAlarmTime;

AlarmInfo alarmInfo =  {STATE_DISARMED, NONE, -1, -1, {-1,-1,-1,-1},};

System node = {
    .runStatus = WAKING_UP,
    .connectionStatus = {
        .wifiIsActive = false,
        .bleIsActive = false,
        .bleConnHandle = -1,
        .bleRemoteWriteHandle = -1,
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
    .buzzer = false
};

// Skicka "State-Heartbeat"
void vTransmitDataTask(void* params){
    uint8_t stateInfo = node.systemState;

    for (;;){
        xQueueReceive(stateQueue, &stateInfo, pdMS_TO_TICKS(5000));
        
        //skicka state över BLE (som "heartbeat")
        sendAlarmState();
    }
}

void vReceiveDataTask(void* params){
    // lokal "behållare" för värdet som tas emot från kön.
    buzzer_init();
    
    for (;;){
        // vänta här - tills något finns i kön.. -> (kopigera då in värdet i 'alarmInfo')
        if (xQueueReceive(alarmQueue, &alarmInfo, portMAX_DELAY)){
            
            // Om skarpt larm -> lämna över till AlarmManagerTask + Yield
            if (alarmInfo.trigger != NONE){

                ESP_LOGI("SensorNode", "Data: %d", alarmInfo.trigger);
                ESP_LOGI("SensorNode", "Time: %d", alarmInfo.time);

                xSemaphoreGive(xAlarmSemaphore);
                taskYIELD();

            } else {
                node.sensorData.indoorTemp = (float)(alarmInfo.climate.inTemp/100.0f);
                node.sensorData.indoorHumidity = (float)(alarmInfo.climate.inHum/100.0f);
                ESP_LOGI("SensorNode", "Indoor Temp: %.1f", node.sensorData.indoorTemp);
                ESP_LOGI("SensorNode", "Indoor Humidity: %.1f%%", node.sensorData.indoorHumidity);
            }
        }
        //vTaskDelay(pdMS_TO_TICKS(100)); // ta bort?
    }
}


void vAlarmManagerTask(void* params){

    for (;;){
        BaseType_t xResult = xSemaphoreTake(xAlarmSemaphore, pdMS_TO_TICKS(2000));

        if (xResult) { 
            // BARA vid semafor
            manageAlarm(); 
        } else {
            // BARA vid timeout
            checkIfReset();

            //if (lastHeartbeatTime >= 0){ ---> ändras till SEND heartbeat, i ny task.
            //    checkHeartbeat();
            //}
            
            
        }
        //vTaskDelay(pdMS_TO_TICKS(20)); // ta bort?
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

    if (node.alarmStatus.ALARM_FIRE){       // högsta prio
        triggerFireAlarmTime = systemTime;
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

    if (node.alarmStatus.ALARM_INTRUSION && !node.alarmStatus.ALARM_FIRE){  // mellan prio
        buzzer_on_fire();
        // Visuellt: Tydligt i display
        // Ljud: hög siren ?

        // SKICKA (Tx):
        // broker -> DB
        // thingsboard ?

        // RESET:
        // inaktiveras när larm inaktiveras av användare - via resetAlarm();
    }

    if (node.alarmStatus.ALARM_WATER && !node.alarmStatus.ALARM_INTRUSION && !node.alarmStatus.ALARM_FIRE){  // låg prio
        triggerWaterAlarmTime = systemTime;
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

void checkIfReset(){
    // reset för brand & vatten (tidsbaserat)

    // om det gått mer än 8s sen brandlarm mottogs -> Reset
    if (systemTime - triggerFireAlarmTime >= FIRE_ALARM_RESET_TIME && node.alarmStatus.ALARM_FIRE == true){
        
        // RESET
        ESP_LOGI("RESET", "FIRE_ALARM");
        node.alarmStatus.ALARM_FIRE = false;
    }

    // om det gått mer än 35s? sen vatten-läckage mottogs -> Reset
    if (systemTime - triggerWaterAlarmTime >= WATER_ALARM_RESET_TIME && node.alarmStatus.ALARM_WATER == true){
        
        // RESET
        ESP_LOGI("RESET", "WATER_ALARM");
        node.alarmStatus.ALARM_WATER = false;
    }


    if (node.alarmStatus.ALARM_FIRE == false && node.alarmStatus.ALARM_INTRUSION == false && node.alarmStatus.ALARM_WATER == false){
        
        if (node.buzzer){
            ESP_LOGI("RESET", "BUZZER");
            buzzer_off();
        }
        
    }
}


void checkHeartbeat(){
    
    if (systemTime - lastHeartbeatTime >= HEARTBEAT_BLE_INTERVALL * 4){ // SensorNode tappar heartbeat vid MQTT fail...kan dröja 15s.
        
        if (node.systemState != STATE_DISARMED){
            // LARMA
            node.alarmStatus.ALARM_INTRUSION = true;
            // info/notis i display -> 'FAIL SAFE'

        } else {
            ESP_LOGI("HEARTBEAT", "FAIL");
            // kort PIP-PIP (samma som water leak..)
            // info/notis i display -> 'FAIL SAFE'
        }
    }
}

void setAlarmState(AlarmState state){
    
    // uppdatera aktuellt state
    node.systemState = state;

    // addera state i kö
    xQueueOverwrite(stateQueue, &state);
}