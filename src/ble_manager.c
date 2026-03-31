#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"    
#include "nvs_flash.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_manager.h"
#include "alarm.h"

uint8_t ble_addr_type;
static const char *TAG = "BLE_CLIENT";

// UUID:s för Arduino (SENS_NODE) - 128-bitars i Little Endian
static const ble_uuid128_t remote_service_uuid = {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0x14, 0x12, 0x8A, 0x76, 0x04, 0xD1, 0x6C, 0x4F, 0x7E, 0x53, 0xF2, 0xE8, 0x00, 0x00, 0xB1, 0x19}
};

static const ble_uuid128_t remote_char_uuid = {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0x14, 0x12, 0x8A, 0x76, 0x04, 0xD1, 0x6C, 0x4F, 0x7E, 0x53, 0xF2, 0xE8, 0x01, 0x00, 0xB1, 0x19}
};



void initBLE(){
    // Tysta kompilatorvarningar för oanvända variabler
    (void)TAG;

    // Initiera NVS 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initiera NimBLE-stacken
    nimble_port_init();
    ble_svc_gap_device_name_set("S3-BLE-Client");
    ble_svc_gap_init();

    // Sätt callback för när stacken är redo
    ble_hs_cfg.sync_cb = ble_app_on_sync;
}



void vBLETask(void *param) {
    nimble_port_run();
}


// Callback när Characteristic hittas -> Aktivera Notify (skriv till CCCD)
static int on_char_disc(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0) {
        ESP_LOGI(TAG,"Aktiverar INDICATE för larmet...\n");
        uint8_t value[] = {0x02, 0x00}; // 0x02 = Indicate
        ble_gattc_write_flat(conn_handle, chr->val_handle + 1, value, sizeof(value), NULL, NULL);
    }
    return 0;
}

// Callback när Service hittas -> Sök efter Characteristic UUID
static int on_service_disc(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
    if (error->status == 0) {
        ESP_LOGI(TAG, "Hittade Service! Söker efter Characteristic...\n");
        ble_gattc_disc_chrs_by_uuid(conn_handle, service->start_handle, service->end_handle, &remote_char_uuid.u, on_char_disc, NULL);
    }
    return 0;
}

// GAP Event Handler - Hanterar Scanning, Connection och Data (Notify)
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != 0) return 0;

            // Logga alla enheter vi ser 
            if (fields.name_len > 0) {              
                // Matcha på namnet "SENS_NODE"
                if (strncmp((char *)fields.name, "SENS_NODE", fields.name_len) == 0) {
                    ESP_LOGI(TAG, "MATCH! Hittade SENS_NODE. Ansluter...\n");
                    ble_gap_disc_cancel();
                    ble_gap_connect(ble_addr_type, &event->disc.addr, 30000, NULL, ble_gap_event, NULL);
                }
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "ANSLUTEN! Startar discovery...\n"); 
                ble_gattc_disc_svc_by_uuid(event->connect.conn_handle, &remote_service_uuid.u, on_service_disc, NULL);
            } else {
                ESP_LOGI(TAG, "Anslutning misslyckades, scannar igen.\n"); 
                ble_app_scan();
            }
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            // data från Sensor-node
            int rc = ble_hs_mbuf_to_flat(event->notify_rx.om, &alarmInfo, sizeof(AlarmInfo), NULL);

            if (event->notify_rx.indication == 1) {
                if (rc == 0){
                    //ESP_LOGI(TAG, "Indicate mottaget! -> Data: %d, Tid: %d", alarmInfo.trigger, alarmInfo.time);
                }
                // skickar larmet in i larmkön
                if (alarmQueue != NULL){
                    xQueueSend(alarmQueue, &alarmInfo, 0);
                } else {
                    ESP_LOGI(TAG, "Misslyckades att packa upp AlarmInfo, rxBLE=%d", rc);
                }
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Frånkopplad. Startar om scanning...\n");
            ble_app_scan();
            break;

        default:
            break;
    }
    return 0;
}

void ble_app_scan(void) {
    struct ble_gap_disc_params disc_params;
    memset(&disc_params, 0, sizeof(disc_params));
    
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;   // ÄNDRA: Kan behöva vara 0 för att se namn..
    disc_params.itvl = 128;    // 80ms interval
    disc_params.window = 64;   // 40ms fönster
    
    ESP_LOGI(TAG, "Försöker starta scanning...\n");
    int rc = ble_gap_disc(ble_addr_type, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGI(TAG, "Fel vid start av scan: rc=%d\n", rc);
    }
}

void ble_app_on_sync(void) {
    printf("NimBLE synkad! Bestämmer adress...\n");
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_scan();
}