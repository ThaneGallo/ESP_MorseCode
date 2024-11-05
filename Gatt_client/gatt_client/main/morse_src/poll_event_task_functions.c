// #include <stdio.h> // we only need this one if we decide to add print statements or similar in here.
#include "poll_event_task_functions.h"
#include "callback_functions.h" // for the callbacks in poll_event_task
#include "morse_functions.h" // for writing to mem and character buffers
// static struct ble_profile *ble_profile1;

// read from server. True = yes, False = no.
bool read_flag = false;
// send to server. True = yes, False = no.
bool send_flag = false;

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

void poll_event_task(void *param) {
    // just ticks tbh
    int cnt = 0;
    while (1)
    {
        int rc; // for error codes
        //printf("cnt: %d\n", cnt++);
        ESP_LOGI(MORSE_TAG,"cnt: %d", cnt++);
        if(send_flag) {
            send_flag = false;
            // ESP_LOGI(DEBUG_TAG,"write_flag true");
            rc = ble_gattc_write_flat(ble_profile1->conn_desc->conn_handle, ble_profile1->characteristic->val_handle, char_message_buf, char_mess_buf_end, ble_gatt_write_chr_cb, NULL);
            char_mess_buf_end = 0;
            mess_buf_end = 0;
            if(rc != 0) {
                ESP_LOGI(ERROR_TAG, "write_event error rc = %d", rc);
                return;
            }
        }
        if(read_flag) {
            read_flag = false;
            // ESP_LOGI(DEBUG_TAG,"read_flag true");
            rc = ble_gattc_read(ble_profile1->conn_desc->conn_handle, ble_profile1->characteristic->val_handle, ble_gatt_read_chr_cb, NULL);
            if(rc != 0) {
                ESP_LOGI(ERROR_TAG, "read_event error rc = %d", rc);
                return;
            }
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