// #include <stdio.h> // we only need this one if we decide to add print statements or similar in here.
#include "poll_event_task_functions.h"

// static struct ble_profile *ble_profile1;

// read from server. True = yes, False = no.
bool read_flag;
// send to server. True = yes, False = no.
bool send_flag;

void poll_event_set_all_flags(bool val) {
    read_flag = val;
    send_flag = val;
}

int poll_event_set_flag(uint8_t flag, bool val) {
    switch(flag) {
        case 1: 
            read_flag = val;
            break;
        case 2:
            send_flag = val;
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
        if(read_flag) {
            read_flag = false;
            ESP_LOGI(DEBUG_TAG,"read_flag true");
            int rc = ble_gattc_read(ble_profile1->conn_desc->conn_handle, ble_profile1->characteristic->val_handle, ble_gatt_read_chr_cb, NULL);
            if(rc != 0) {
                ESP_LOGI(ERROR_TAG, "read_event error rc = %d", rc);
                return;
            }
        }
        if(send_flag) {
            // do something (add later)
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// COMMENTED BECAUSE MORSE_COMMON.C NOW GLOBALLY INITIALIZES THE BLE_PROFILE
// int poll_event_set_profile_ptr(ble_profile *profile_ptr) {
//     if(!profile_ptr) {
//         ESP_LOGI(ERROR_TAG,"no profile_ptr passed to poll_event");
//         return -1;
//     }
//     ble_profile1 = profile_ptr;
//     return 0;
// }