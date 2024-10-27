#ifndef TESTING_FUNCTIONS_H
#define TESTING_FUNCTIONS_H

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdbool.h>

#define POLL_EVENT_READ_FLAG 1
#define POLL_EVENT_WRITE_FLAG 2

typedef struct ble_profile
{
    const struct ble_gap_conn_desc *conn_desc;
    const struct ble_gatt_svc *service;
    // struct ble_gatt_chr characteristic[CHARACTERISTIC_ARR_MAX]; // characteristic array holds all the characteristics.
    struct ble_gatt_chr *characteristic; // characteristic array holds all the characteristics.
} ble_profile;

/**
 * Sets all flags to the value given.
 */
void poll_event_set_all_flags(bool val);

/**
 * Set a particular flag based on the following key.
 * @param flag the flag to set. 1 == read_flag, 2 == write_flag.
 * @param val the boolean to set the flag to. true or false.
 * @return 0 on success, -1 on error.
 */
int poll_event_set_flag(uint8_t flag, bool val);

/**
 * Checks for any event by monitoring certain flags. If any flag is true, then triggers correct task.
 */
void poll_event_task(void *param);

/**
 * Sets the profile pointer internally for other functions to use.
 * @return 0 on success, !0 on failure.
 */
int poll_event_set_profile_ptr(ble_profile *profile_ptr);




#endif