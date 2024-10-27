// #include <stdio.h> // we only need this one if we decide to add print statements or similar in here.
#include "testing_functions.h"


#define MORSE_TAG "Morse code tag"
#define DEBUG_TAG "Debugging tag"
#define ERROR_TAG "||| ERROR |||"

// DELETE THIS
// ____________________________________________________
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_event.h"
#include "esp_nimble_hci.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static struct ble_profile *ble_profile1;
// ____________________________________________________

// read from server. True = yes, False = no.
bool read_flag;
// write to server. True = yes, False = no.
bool write_flag;

void poll_event_set_all_flags(bool val) {
    read_flag = val;
    write_flag = val;
}

int poll_event_set_flag(uint8_t flag, bool val) {
    switch(flag) {
        case 1: 
            read_flag = val;
            break;
        case 2:
            write_flag = val;
            break;
        default:
            ESP_LOGI(ERROR_TAG,"invalid flag %d", flag);
            return -1;
    }
    return 0;
}

/**
 * Callback function for gatt read events.
 */
static int ble_gatt_read_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
    // READ EVENTS
    if(error->status != 0) {
        ESP_LOGI(DEBUG_TAG, "ble_gatt_disc_attr_cb error = [handle, status] = [%d, %d]", error->att_handle, error->status);
        return -1;
    }
    // grab the data and print it.
    ESP_LOGI(MORSE_TAG, "Data from the client: %.*s\n", attr->om->om_len, attr->om->om_data);
    return 0;
}

void poll_event_task(void *param) {
    // just ticks tbh
    int cnt = 0;
    while (1)
    {
        //printf("cnt: %d\n", cnt++);
        ESP_LOGI(MORSE_TAG,"cnt: %d", cnt++);
        if(read_flag == true) {
            read_flag = false;
            ESP_LOGI(DEBUG_TAG,"read_flag true");
            int rc = ble_gattc_read(ble_profile1->conn_desc->conn_handle, ble_profile1->characteristic->val_handle, ble_gatt_read_chr_cb, NULL);
            if(rc != 0) {
                ESP_DRAM_LOGI(ERROR_TAG, "read_event error rc = %d", rc);
                return;
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

int poll_event_set_profile_ptr(ble_profile *profile_ptr) {
    if(!profile_ptr) {
        ESP_LOGI(ERROR_TAG,"no profile_ptr passed to poll_event");
        return -1;
    }
    ble_profile1 = profile_ptr;
    return 0;
}