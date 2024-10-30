#ifndef CALLBACK_FUNCTIONS_H
#define CALLBACK_FUNCTIONS_H

#include "morse_common.h"


/**
 * Callback function for gatt service discovery.
 */
int ble_gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);

/**
 * Callback function for gatt characteristic discovery.
 */
int ble_gatt_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg);

/**
 * Callback function for gatt read events.
 */
int ble_gatt_read_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);

#endif