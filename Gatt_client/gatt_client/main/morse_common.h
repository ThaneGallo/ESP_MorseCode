#ifndef MORSE_COMMON_H
#define MORSE_COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

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

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>


#define MORSE_TAG "Morse code tag"
#define DEBUG_TAG "Debugging tag"
#define ERROR_TAG "||| ERROR |||"

#define CHARACTERISTIC_ARR_MAX 1

typedef struct ble_profile
{
    const struct ble_gap_conn_desc *conn_desc;
    const struct ble_gatt_svc *service;
    // struct ble_gatt_chr characteristic[CHARACTERISTIC_ARR_MAX]; // characteristic array holds all the characteristics.
    struct ble_gatt_chr *characteristic; // characteristic array holds all the characteristics.
} ble_profile;

// static struct ble_profile *ble_profile1;
extern struct ble_profile *ble_profile1;

const ble_addr_t *ble_server_addr_return();
const ble_addr_t *ble_client_addr_return();

void debug_print_conn_desc(struct ble_gap_conn_desc *conn_desc_ptr);

#endif