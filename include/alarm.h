#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H
#include <stdint.h> 

void vAlarmReceiveTask(void* params);
void manageAlarm();
void setAlarm();

// ======= SYSTEM STATUS =======
typedef enum
{
    WAKING_UP,
    RUNNING
}RunStatus;

// ======= CONNECTION STATUS =======
typedef struct {
    bool wifiIsActive;
    bool bleIsActive;
    bool mqttIsActive;
}ConnectionStatus;

// ======= ALARM INFO::TRIGGERS =======
typedef enum : uint8_t
{
    NONE = 0,       // = Heartbeat
    WATER = 1,
    DOOR = 2,
    MOTION = 3,
    FIRE = 4
}AlarmTrigger;


// ======= ALARM INFO (Rx) =======
// packad strukt, ta emot larm. [Från Arduino eller Zero]
typedef struct __attribute__((packed))
{
    AlarmTrigger trigger;
    uint32_t time;
}AlarmInfo;

extern AlarmInfo alarmInfo;

// ======= ALARM STATE =======
typedef enum
{
    STATE_DISARMED,
    STATE_ARMED_HOME,
    STATE_ARMED_AWAY
}AlarmState; //alarm_state_t; (ändra!)

typedef struct
{
    bool ALARM_INTRUSION;     // Larmar, inbrott
    bool ALARM_FIRE;        // Larmar, brand
    bool ALARM_WATER;
}AlarmingStatus;


// ======= WETHER API =======
typedef struct{
    float API_temp;
    float API_humid;
    char API_description[100]; 
}OpenMeteo;


// ======= SENSOR DATA =======
typedef struct
{
    float indoorTemp;
    float indoorHumidity;
    OpenMeteo wether;
}SensorData;


// =======================
// ==== STATE MACHINE ==== 
// =======================
typedef struct
{
    RunStatus runStatus;                // WAKING_UP | RUNNING --> (används ej än)
    ConnectionStatus connectionStatus;  // WiFi Active? | BLE Active? | MQTT Active?
    AlarmState systemState;              // STATE_DISARMED | STATE_ARMED_HOME | STATE_ARMED_AWAY
    AlarmingStatus alarmStatus;            // FAIL | IDLE | PENDING | ALARMING
    SensorData sensorData;              // API 'OpenWether' & DHT11
    volatile unsigned long sysTime;     // System-tiden
}System;

// deklarera variabel för systemet
extern System node;




#endif