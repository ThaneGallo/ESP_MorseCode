#include "callback_functions.h"

int ble_gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
{

    ESP_LOGI(MORSE_TAG, "start of svc callback");

    struct ble_profile *profile_ptr = (struct ble_profile *)arg; // typecast arg to be a ble_profile
    uint8_t err = 0;

    if (!profile_ptr)
    {
        ESP_LOGI(MORSE_TAG, "profile ptr is null");
        return -1;
    }

    if(!service) {
        ESP_LOGI(DEBUG_TAG, "ble_gatt_disc_svc: service data empty. handle %u, status %u", error->att_handle, error->status);
    }

    // check if there is an error
    switch(error->status) {
        case 0: {
            ESP_LOGI(DEBUG_TAG, "ble_gatt_disc_svc: no error, attr_handle: %u, status: %u", error->att_handle, error->status);
            break;
        }
        case BLE_HS_EDONE: {
            ESP_LOGI(DEBUG_TAG, "ble_gatt_disc_svc: all done, status %u", error->status);
            return 0;
        }
        default: {
            ESP_LOGI(ERROR_TAG, "ble_gatt_disc_svc: an error has occured: attr handle:%u status -> %u", error->att_handle, error->status);
            return error->status;
        }
    }

    // check that service is real
    if (!service) 
    {
        ESP_LOGI(ERROR_TAG, "ble_gatt_disc_svc: an error has occured: service is not real");
        return -1;
    } else if (service->uuid.u16.value == 0x1800 || service->uuid.u16.value == 0x1801) {
        ESP_LOGI(DEBUG_TAG, "uuid of generic service = %04x", service->uuid.u16.value);
        return 0; // these are generic characteristics and attributes, per our google investigation
    }

    //ESP_LOGI(DEBUG_TAG, "uuid of service = %04x", service->uuid.u16.value);

    ESP_LOGI(DEBUG_TAG, "uuid of service = %8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x", service->uuid.u128.value[0], service->uuid.u128.value[1], service->uuid.u128.value[2],
        service->uuid.u128.value[3], service->uuid.u128.value[4], service->uuid.u128.value[5], service->uuid.u128.value[6], service->uuid.u128.value[7], service->uuid.u128.value[8],
        service->uuid.u128.value[9], service->uuid.u128.value[10], service->uuid.u128.value[11], service->uuid.u128.value[12], service->uuid.u128.value[13], 
        service->uuid.u128.value[14], service->uuid.u128.value[15]
    );

    profile_ptr->service = service;

    //  ESP_LOGI(DEBUG_TAG, "", rc);

    // ESP_LOGI(MORSE_TAG, "after service alloc");

    // discover all characteristics
    err = ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle, ble_gatt_chr_cb, profile_ptr);
    if (err != 0)
    {
        ESP_LOGI(ERROR_TAG, "ble_gattc_disc_all_chrs: an error has occured: %u", err);
        return err;
    }

    return 0;
}

int ble_gatt_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
{

    ESP_LOGI(DEBUG_TAG, "start of chr callback");
    struct ble_profile *profile_ptr = (struct ble_profile *)arg;

    if(!chr) {
        ESP_LOGI(DEBUG_TAG, "ble_gatt_chr_cb: characteristic data empty. handle %u, status %u", error->att_handle, error->status);
    }

    // check if there is an error
    switch(error->status) {
        case 0: {
            ESP_LOGI(DEBUG_TAG, "ble_gatt_chr_cb: no error, attr_handle: %u, status: %u", error->att_handle, error->status);
            break;
        }
        case BLE_HS_EDONE: {
            ESP_LOGI(DEBUG_TAG, "ble_gatt_chr_cb: all done, status %u", error->status);
            return 0;
        }
        default: {
            ESP_LOGI(ERROR_TAG, "ble_gatt_chr_cb: an error has occured: error %u -> %u", error->att_handle, error->status);
            return error->status;
        }
    }

    // ESP_LOGI(DEBUG_TAG, "uuid of characteristic = %04x", chr->uuid.u16.value);
    // ESP_LOGI(DEBUG_TAG, "uuid of characteristic = %32x", chr->uuid.u128.value);
    ESP_LOGI(DEBUG_TAG, "uuid of characteristic = %8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x%8x", chr->uuid.u128.value[0], chr->uuid.u128.value[1], 
        chr->uuid.u128.value[2], chr->uuid.u128.value[3], chr->uuid.u128.value[4], chr->uuid.u128.value[5], chr->uuid.u128.value[6], 
        chr->uuid.u128.value[7], chr->uuid.u128.value[8], chr->uuid.u128.value[9], chr->uuid.u128.value[10], chr->uuid.u128.value[11], 
        chr->uuid.u128.value[12], chr->uuid.u128.value[13], chr->uuid.u128.value[14], chr->uuid.u128.value[15]
    );

    static uint8_t characteristic_count = 0; // characteristic_count as static, persists between function calls.
    profile_ptr->characteristic[characteristic_count] = *chr; // save the latest characteristic data to this local profile_ptr

    // increment the count until we are at max.
    if (characteristic_count < CHARACTERISTIC_ARR_MAX)
    {
        characteristic_count++;
    }
    else
    {
        ESP_LOGI(ERROR_TAG, "ble_gatt_chr_cb: characteristic count exceeded maximum for handle %u", conn_handle);
        return 0x69; // our own error code for if the characteristic count has exceeded
    }

    return 0;
}

int ble_gatt_read_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
    // READ EVENTS
    if(error->status != 0) {
        ESP_LOGI(DEBUG_TAG, "ble_gatt_disc_attr_cb error = [handle, status] = [%d, %d]", error->att_handle, error->status);
        return -1;
    }
    // grab the data and print it.
    ESP_LOGI(MORSE_TAG, "Data from the client: %.*s\n", attr->om->om_len, attr->om->om_data);
    return 0;
}
