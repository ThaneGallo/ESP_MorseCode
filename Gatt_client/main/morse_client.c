/*modified 10/30/2024*/
#include "morse_common.h"
#include "morse_functions.h"
#include "poll_event_task_functions.h"
#include "callback_functions.h"

// DISCOVERY PARAMETERS FOR GAP SEARCH
static struct ble_gap_disc_params disc_params = {
    .filter_duplicates = 1,
    .passive = 0,
    .itvl = 0,
    .window = 0,
    .filter_policy = BLE_HCI_SCAN_FILT_USE_WL,
    //.filter_policy = BLE_HCI_SCAN_FILT_NO_WL, // find all things no whitelist used.
    .limited = 0
};

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
 * Helper function for gap discovery events to make ble_gap_event more readable
 */
void ble_gap_disc_event_helper(uint8_t err) {
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
    return;
}

/**
 * Helper function for gap connection events to make ble_gap_event more readable
 */
int ble_gap_connect_event_helper(struct ble_gap_event *event, void *arg, struct ble_profile *profile_ptr) {
    ESP_LOGI(MORSE_TAG, "ble_gap_event_connect");

    uint8_t err = ble_gap_conn_active();
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
        return -1;
    }
    ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address successful");

    // debugPrintserver_desc();
    gatt_conn_init(profile_ptr);
    return 0;
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

    uint8_t err;
    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        // Handle device discovery
        ESP_LOGI(MORSE_TAG, "Device found: %x%x%x%x%x%x", event->disc.addr.val[0], event->disc.addr.val[1],
                    event->disc.addr.val[2], event->disc.addr.val[3], event->disc.addr.val[4], event->disc.addr.val[5]);
        // Connect to the device if it matches your criteria
        // Replace `event->disc.addr` with the address of the device you want to connect to
        ble_gap_disc_cancel(); // cancel discovery to allow for connection

        // err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, server_ptr, 10000, NULL, NULL, NULL);
        err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, &event->disc.addr, 10000, NULL, ble_gap_event, profile_ptr); // works just fine.
        // err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, NULL, 10000, NULL, NULL, NULL);

        ble_gap_disc_event_helper(err);
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(MORSE_TAG, "Discover event complete");
        break;
    case BLE_GAP_EVENT_CONNECT:
        err = ble_gap_connect_event_helper(event, arg, profile_ptr);
        if(err != 0) {
            return err;
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(MORSE_TAG, "ble_gap_event_disconnect successful, restarting to establish new connection.");
        esp_restart();
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

    gpio_isr_handler_add(GPIO_INPUT_IO_SEND, gpio_send_event_handler, (void *)GPIO_INPUT_IO_SEND);
    // gpio_isr_handler_add(GPIO_INPUT_IO_SEND, gpio_read_event_handler, (void *)GPIO_INPUT_IO_SEND);
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

    uint8_t white_list_count = 1;
    err = ble_gap_wl_set(ble_server_addr_return(), white_list_count); // sets white list for connection to other device
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set whitelist failed");
    }


    // begin gap discovery
    err = ble_gap_disc(BLE_OWN_ADDR_RANDOM, BLE_HS_FOREVER, &disc_params, ble_gap_event, profile);
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
