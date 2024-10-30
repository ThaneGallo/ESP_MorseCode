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

void debug_print_conn_desc(struct ble_gap_conn_desc *conn_desc_ptr)
{
    // each ble_addr_t has type and val
    ESP_LOGI(DEBUG_TAG, "our_id_addr: type = %x, val = %x%x%x%x%x%x", conn_desc_ptr->our_id_addr.type, conn_desc_ptr->our_id_addr.val[0], conn_desc_ptr->our_id_addr.val[1],
             conn_desc_ptr->our_id_addr.val[2], conn_desc_ptr->our_id_addr.val[3], conn_desc_ptr->our_id_addr.val[4], conn_desc_ptr->our_id_addr.val[5]);
    ESP_LOGI(DEBUG_TAG, "peer_id_addr: type = %x, val = %x%x%x%x%x%x", conn_desc_ptr->peer_id_addr.type, conn_desc_ptr->peer_id_addr.val[0], conn_desc_ptr->peer_id_addr.val[1],
             conn_desc_ptr->peer_id_addr.val[2], conn_desc_ptr->peer_id_addr.val[3], conn_desc_ptr->peer_id_addr.val[4], conn_desc_ptr->peer_id_addr.val[5]);
    ESP_LOGI(DEBUG_TAG, "our_ota_addr: type = %x, val = %x%x%x%x%x%x", conn_desc_ptr->our_ota_addr.type, conn_desc_ptr->our_ota_addr.val[0], conn_desc_ptr->our_ota_addr.val[1],
             conn_desc_ptr->our_id_addr.val[2], conn_desc_ptr->our_ota_addr.val[3], conn_desc_ptr->our_ota_addr.val[4], conn_desc_ptr->our_ota_addr.val[5]);
    ESP_LOGI(DEBUG_TAG, "peer_ota_addr: type = %x, val = %x%x%x%x%x%x", conn_desc_ptr->peer_ota_addr.type, conn_desc_ptr->peer_ota_addr.val[0], conn_desc_ptr->peer_ota_addr.val[1],
             conn_desc_ptr->peer_ota_addr.val[2], conn_desc_ptr->peer_ota_addr.val[3], conn_desc_ptr->peer_ota_addr.val[4], conn_desc_ptr->peer_ota_addr.val[5]);

    // ESP_LOGI(DEBUG_TAG, "our_id_addr: type = %x", conn_desc_ptr->our_id_addr.type); // Control test debug
}