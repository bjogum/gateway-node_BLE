#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H
#include <stdint.h> 

void vAlarmReceiveTask(void* params);

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

// ======= ALARM TRIGGER STATES =======
typedef enum : uint8_t
{
    NONE = 0,       // = Heartbeat
    WATER = 1,
    DOOR = 2,
    MOTION = 3,
    FIRE = 4
}AlarmTrigger;


// ======= ALARM INFO =======
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


// ======= ALARM STATUS =======
typedef enum
{
    FAIL,       // Felläge
    IDLE,       // Normalläge
    PENDING,    // Ex. dörr öppnad - på väg att larma av (pip-ljud för uppmärksamhet) - efter 30s går larm.
    ALARMING    // Larmar!
}AlarmStatus;

// ======= WETHER API =======
typedef struct{
    float API_temp;
    float API_humid;
    char API_description[100]; 
}OpenWether;

// ======= SENSOR DATA =======
typedef struct
{
    float indoorTemp;
    float indoorHumidity;
    OpenWether openWether;
}SensorData;


// =======================
// ==== STATE MACHINE ==== 
// =======================
typedef struct
{
    RunStatus runStatus;                // WAKING_UP | RUNNING --> (används ej än)
    ConnectionStatus connectionStatus;  // WiFi Active? | BLE Active? | MQTT Active?
    AlarmState alarmState;              // STATE_DISARMED | STATE_ARMED_HOME | STATE_ARMED_AWAY
    AlarmStatus alarmStatus;            // FAIL | IDLE | PENDING | ALARMING
    SensorData sensorData;              // API 'OpenWether' & DHT11
    volatile unsigned long sysTime;     // System-tiden
}System;

// deklarera variabel för systemet
extern System node;




#endif