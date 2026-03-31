#ifndef BLEMANAGER_H
#define BLEMANAGER_H
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t alarmQueue;
//static int ble_gap_event(struct ble_gap_event *event, void *arg);

void vBLETask(void *param);
void ble_app_scan(void);
void ble_app_on_sync(void);
void initBLE();


#endif