#include "morse_common.h"

// ble address structs and pointers
// static const ble_addr_t server_addr = {
//     .type = BLE_ADDR_RANDOM,
//     .val = {0xDE, 0xCA, 0xFB, 0xEE, 0xFE, 0xD2}
// };

// static const ble_addr_t client_addr = {
//     .type = BLE_ADDR_RANDOM,
//     .val = {0xCA, 0xFF, 0xED, 0xBE, 0xEE, 0xEF}
// };

// declaration of memory for our profile in all inheriting files.
struct ble_profile *ble_profile1;

const ble_addr_t server_addr = {
    .type = BLE_ADDR_RANDOM,
    .val = {0xDE, 0xCA, 0xFB, 0xEE, 0xFE, 0xD2}
};

const ble_addr_t client_addr = {
    .type = BLE_ADDR_RANDOM,
    .val = {0xCA, 0xFF, 0xED, 0xBE, 0xEE, 0xEF}
};

const ble_addr_t *ble_server_addr_return(){
    return &server_addr; 
}

const ble_addr_t *ble_client_addr_return(){
    return &client_addr; 
}