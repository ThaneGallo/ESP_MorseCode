/*modified 10/30/2024*/
#include "morse_common.h"
#include "morse_functions.h"
#include "poll_event_task_functions.h"


static uint8_t white_list_count = 1;

#define CHARACTERISTIC_ARR_MAX 1
static uint8_t characteristic_count = 0;



// DISCOVERY PARAMETERS FOR GAP SEARCH
static struct ble_gap_disc_params disc_params = {
    .filter_duplicates = 1,
    .passive = 0,
    .itvl = 0,
    .window = 0,
    .filter_policy = BLE_HCI_SCAN_FILT_USE_WL,
    //.filter_policy = BLE_HCI_SCAN_FILT_NO_WL, // find all things no whitelist used.
    .limited = 0};


// forward declarations
static int ble_gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);




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




/**
 * Callback function for gatt characteristic discovery.
 */
static int ble_gatt_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
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

/**
 * Callback function for gatt service discovery.
 */
static int ble_gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
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

/**
 * Find the service, return the handle of the connection (service? attribute?), setup callbacks for services & get ball running
 */
void gatt_conn_init(struct ble_profile *profile)
{
    uint8_t err;
    ESP_LOGI(DEBUG_TAG, "before disc all srv");

    if (!profile)
    {
        ESP_LOGI(ERROR_TAG, "Null pointer on line %d", __LINE__);
        return;
    }

    err = ble_gattc_disc_all_svcs(profile->conn_desc->conn_handle, ble_gatt_disc_svc_cb, profile); // discover all primary services
    switch (err)
    {
    case 0:
        ESP_LOGI(MORSE_TAG, "gattc service discovery successful");
        break;
    default:
        ESP_LOGI(MORSE_TAG, "gattc service discovery failed, err = %u", err);
        break;
    }
    // save the global version of the profile pointer
    ble_profile1 = profile;
    //poll_event_set_profile_ptr(ble_profile1); // outdated since it is now universal.
}

/**
 * Callback function for gap events.
 */
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_profile *profile_ptr = (struct ble_profile *)arg;
    if (!profile_ptr)
    {
        ESP_LOGI(ERROR_TAG, "Null pointer on line %d", __LINE__);
        return -1;
    }

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        // Handle device discovery
        ESP_LOGI(MORSE_TAG, "Device found: %x%x%x%x%x%x", event->disc.addr.val[0], event->disc.addr.val[1],
                 event->disc.addr.val[2], event->disc.addr.val[3], event->disc.addr.val[4], event->disc.addr.val[5]);
        // Connect to the device if it matches your criteria
        // Replace `event->disc.addr` with the address of the device you want to connect to
        ble_gap_disc_cancel(); // cancel discovery to allow for connection
        uint8_t err;

        // err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, server_ptr, 10000, NULL, NULL, NULL);
        err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, &event->disc.addr, 10000, NULL, ble_gap_event, profile_ptr); // works just fine.
        // err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, NULL, 10000, NULL, NULL, NULL);

        switch (err)
        {
        case 0:
            ESP_LOGI(MORSE_TAG, "ble_gap_connect successful");
            break;
        case BLE_HS_EALREADY:
            ESP_LOGI(MORSE_TAG, "ble_gap_connect connection already in progress");
            break;
        case BLE_HS_EBUSY:
            ESP_LOGI(MORSE_TAG, "ble_gap_connect connection not possible because scanning is in progress");
            break;
        case BLE_HS_EDONE:
            ESP_LOGI(MORSE_TAG, "ble_gap_connect specified peer is already connected");
            break;
        default:
            ESP_LOGI(MORSE_TAG, "ble_gap_connect other nonzero on error: %u", err);
        }
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(MORSE_TAG, "Discover event complete");
        break;
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(MORSE_TAG, "ble_gap_event_connect");

        err = ble_gap_conn_active();
        ESP_LOGI(MORSE_TAG, "BLE Active Connection Procedure = %d", err);

        ESP_LOGI(MORSE_TAG, "BLE Connection Status = %d", event->connect.status);
        ESP_LOGI(MORSE_TAG, "BLE Connection Handle = %x", event->connect.conn_handle);

        err = ble_gap_conn_find(event->connect.conn_handle, profile_ptr->conn_desc);
        // err = ble_gap_conn_find_by_addr(server_ptr, server_desc); // setup server_desc with the data SUCCESSFUL
        // err = ble_gap_conn_find_by_addr(&event->disc.addr, server_desc); // setup server_desc with the data

        if (!profile_ptr->conn_desc)
        {
            ESP_LOGI(ERROR_TAG, "Null pointer on line %d", __LINE__);
            return -1;
        }

        if (err != 0)
        {
            if (err == BLE_HS_EDISABLED)
            {
                ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address Failed, Operation Disabled.");
            }
            else if (err == BLE_HS_ENOTCONN)
            {
                ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address Failed, Not Connected");
            }
            else
            {
                ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address Failed, error code: %d", err);
            }
            break;
        }
        ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address successful");

        // debugPrintserver_desc();

        gatt_conn_init(profile_ptr);

        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(MORSE_TAG, "ble_gap_event_disconnect successful");
        break;
    default:
        ESP_LOGI(MORSE_TAG, "Called Event without handler: %u", event->type);
        break;
    }

    return 0;
}

void gpio_setup()
{

    // zero-initialize the config structure.
    gpio_config_t io_conf = {};

    // interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // bit mask of the pins, use GPIO 4 & 23 here
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_START) | (1ULL << GPIO_INPUT_IO_SEND);
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable high on default
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    // bit mask of the pins, use GPIO 5 here
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_END);
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable high on default
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // takes start time of button1 press
    gpio_isr_handler_add(GPIO_INPUT_IO_START, gpio_start_event_handler, (void *)GPIO_INPUT_IO_START);
    // takes end time of button1 pess
    gpio_isr_handler_add(GPIO_INPUT_IO_END, gpio_end_event_handler, (void *)GPIO_INPUT_IO_END);

    // gpio_isr_handler_add(GPIO_INPUT_IO_SEND, gpio_send_event_handler, (void *)GPIO_INPUT_IO_SEND);
    gpio_isr_handler_add(GPIO_INPUT_IO_SEND, gpio_read_event_handler, (void *)GPIO_INPUT_IO_SEND);
}

// ble_task which runs infinitely and checks for ble_events.
void ble_task(void *param)
{
    nimble_port_run();
    return;
}

void ble_app_on_sync(void)
{
    // *********create the profile structure, allocate memory, and pass it the characteristic data*********
    ble_profile *profile;
    profile = malloc(sizeof(struct ble_profile));

    profile->conn_desc = malloc(sizeof(struct ble_gap_conn_desc));
    profile->service = malloc(sizeof(struct ble_gatt_svc));
    // create a pointer to ble_gatt_chr pointers, to be used as array.
    profile->characteristic = malloc(sizeof(struct ble_gatt_chr) * CHARACTERISTIC_ARR_MAX);
    if (!profile->conn_desc)
    {
        ESP_LOGI(ERROR_TAG, "BLE conn_desc is NULL on line %d", __LINE__);
        free(profile->conn_desc);
    }
    if (!profile->service)
    {
        ESP_LOGI(ERROR_TAG, "BLE service is NULL on line %d", __LINE__);
        free(profile->service);
    }
    if (!profile->characteristic)
    {
        ESP_LOGI(ERROR_TAG, "BLE Characteristic is NULL on line %d", __LINE__);
        free(profile->characteristic);
    }
    
    uint8_t err;
    err = ble_hs_id_set_rnd(ble_client_addr_return()->val);
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set random address failed %d", err);
    }

    err = ble_gap_wl_set(ble_server_addr_return(), white_list_count); // sets white list for connection to other device
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set whitelist failed");
    }


    uint8_t ble_addr_type = BLE_OWN_ADDR_RANDOM;

    // begin gap discovery
    err = ble_gap_disc(ble_addr_type, 10 * 1000, &disc_params, ble_gap_event, profile);
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE GAP Discovery Failed: %u", err);
    }
}



void ble_client_setup()
{
    uint8_t err;

    // init
    ESP_ERROR_CHECK(nvs_flash_init()); // sets up flash memory
    // ESP_ERROR_CHECK(esp_nimble_hci_init()); // dies here
    ESP_ERROR_CHECK(nimble_port_init());

    err = ble_svc_gap_device_name_set("BLE-Scan-Client"); // 4 - Set device name characteristic
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "GAP device name set");
    }

    ble_svc_gap_init(); // initialize gap
    ble_gattc_init();   // initialize gatt

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    xTaskCreate(poll_event_task, "Poll Event Task", 2048, NULL, 5, NULL);

    // starts first task
    nimble_port_freertos_init(ble_task);
}

void app_main(void)
{
    gpio_setup();
    ble_client_setup();
}
