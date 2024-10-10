/*modified 10/9/2024*/

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

#include "morse_functions.h"

static uint8_t white_list_count = 1;

// gpio
static uint32_t mess_buf_end = 0;
static uint32_t char_mess_buf_end = 0;

static int64_t start_time;          // time of last valid start
static int64_t time_last_end_event; // time of last valid end
static bool input_in_progress;

#define PRESS_LENGTH 1000000 // Hold-time required for dash '-' input. 1 second in microseconds
#define SPACE_LENGTH 2000000 // Time required between inputs for new character start. 2 seconds in microseconds

// START, END, AND SEND EVENTS
#define GPIO_INPUT_IO_START 4 // start event sense
#define GPIO_INPUT_IO_END 5   // end event sense
#define GPIO_INPUT_IO_SEND 23 // send event sense

#define MESS_BUFFER_LENGTH 2048
#define CHAR_BUFFER_LENGTH 256
static uint8_t message_buf[MESS_BUFFER_LENGTH];
static char char_message_buf[CHAR_BUFFER_LENGTH];

#define ESP_INTR_FLAG_DEFAULT 0
#define MORSE_TAG "Morse code tag"
#define DEBUG_TAG "Debugging tag"
#define ERROR_TAG "||| ERROR |||"
#define DEBOUNCE_DELAY 50000 // time required between consecutive inputs to prevent debounce issues

// ble address structs and pointers
static const ble_addr_t server_addr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xDE, 0xCA, 0xFB, 0xEE, 0xFE, 0xD2}
    //.val = {0xD0, 0xFE, 0xEE, 0xFB, 0xCA, 0xDE}
};

static const ble_addr_t client_addr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xCA, 0xFF, 0xED, 0xBE, 0xEE, 0xEF}
    //.val = {0xEF, 0xEE, 0xBE, 0xED, 0xFF, 0xCA}
};

static uint8_t ble_addr_type = BLE_OWN_ADDR_RANDOM;
// const uint8_t *clientaddressVal = &client_addr.val;
static const ble_addr_t *server_ptr = &server_addr;
static const ble_addr_t *client_ptr = &client_addr;

static struct ble_gap_conn_desc server_desc;
static struct ble_gap_conn_desc *server_desc_ptr = &server_desc;

// static struct ble_gatt_svc server_service;
// static struct ble_gatt_svc *server_service_ptr = &server_service;

#define CHARACTERISTIC_ARR_MAX 2
// static struct ble_gatt_chr *characteristics[CHARACTERISTIC_ARR_MAX]; // to store the characteristics
static uint8_t characteristic_count = 0;

typedef struct ble_profile
{
    const struct ble_gap_conn_desc *conn_desc;
    const struct ble_gatt_svc *service;
    // struct ble_gatt_chr characteristic[CHARACTERISTIC_ARR_MAX]; // characteristic array holds all the characteristics.
    struct ble_gatt_chr *characteristic; // characteristic array holds all the characteristics.
} ble_profile;

// DISCOVERY PARAMETERS FOR GAP SEARCH
static struct ble_gap_disc_params disc_params = {
    .filter_duplicates = 1,
    .passive = 0,
    .itvl = 0,
    .window = 0,
    .filter_policy = BLE_HCI_SCAN_FILT_USE_WL,
    //.filter_policy = BLE_HCI_SCAN_FILT_USE_WL_INITA, // error code 18.
    //.filter_policy = BLE_HCI_SCAN_FILT_NO_WL, // find all things no whitelist used.
    .limited = 0};

// UUID macros
// #define SERVICE_UUID 0xCAFE
// #define READ_UUID 0xCAFF
// #define WRITE_UUID 0xDECA
static uint8_t SERVICE_UUID[16] = {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE};
static uint8_t READ_UUID[16] = {0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF, 0xCA, 0xFF};
static uint8_t WRITE_UUID[16] = {0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA, 0xDE, 0xCA};

// static uint8_t write_val = WRITE_UUID;
// static uint8_t service_val = SERVICE_UUID;

// forward declarations
static int ble_gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);

/**
 * Print the contents of both the message and character buffers into the terminal.
 */
void debugPrintBuffer()
{
    int i;

    for (i = 0; i <= mess_buf_end; i++)
    {
        ESP_DRAM_LOGD(DEBUG_TAG, "message buffer[%d]: %d", i, message_buf[i]);
    }

    for (i = 0; i <= char_mess_buf_end; i++)
    {
        ESP_DRAM_LOGD(DEBUG_TAG, "character buffer[%d]: %c", i, char_message_buf[i]);
    }
}

void debugPrintserver_desc()
{
    // each ble_addr_t has type and val
    ESP_LOGI(DEBUG_TAG, "our_id_addr: type = %x, val = %x%x%x%x%x%x", server_desc_ptr->our_id_addr.type, server_desc_ptr->our_id_addr.val[0], server_desc_ptr->our_id_addr.val[1],
             server_desc_ptr->our_id_addr.val[2], server_desc_ptr->our_id_addr.val[3], server_desc_ptr->our_id_addr.val[4], server_desc_ptr->our_id_addr.val[5]);
    ESP_LOGI(DEBUG_TAG, "peer_id_addr: type = %x, val = %x%x%x%x%x%x", server_desc_ptr->peer_id_addr.type, server_desc_ptr->peer_id_addr.val[0], server_desc_ptr->peer_id_addr.val[1],
             server_desc_ptr->peer_id_addr.val[2], server_desc_ptr->peer_id_addr.val[3], server_desc_ptr->peer_id_addr.val[4], server_desc_ptr->peer_id_addr.val[5]);
    ESP_LOGI(DEBUG_TAG, "our_ota_addr: type = %x, val = %x%x%x%x%x%x", server_desc_ptr->our_ota_addr.type, server_desc_ptr->our_ota_addr.val[0], server_desc_ptr->our_ota_addr.val[1],
             server_desc_ptr->our_id_addr.val[2], server_desc_ptr->our_ota_addr.val[3], server_desc_ptr->our_ota_addr.val[4], server_desc_ptr->our_ota_addr.val[5]);
    ESP_LOGI(DEBUG_TAG, "peer_ota_addr: type = %x, val = %x%x%x%x%x%x", server_desc_ptr->peer_ota_addr.type, server_desc_ptr->peer_ota_addr.val[0], server_desc_ptr->peer_ota_addr.val[1],
             server_desc_ptr->peer_ota_addr.val[2], server_desc_ptr->peer_ota_addr.val[3], server_desc_ptr->peer_ota_addr.val[4], server_desc_ptr->peer_ota_addr.val[5]);

    // ESP_LOGI(DEBUG_TAG, "our_id_addr: type = %x", server_desc_ptr->our_id_addr.type); // Control test debug
}

/**
 * Converts message_buf values into corresponding characterBuffer values.
 */
void encodeMorseCode()
{
    /*
    - For each bit-letter-combo in the message_buf array,
    - grab the bits for each letter individually, stopping when you reach the '2' at the end of the input
    - translate that into the corresponding character using getLetterMorseCode()
    - save that character into the character buffer
    */

    int currentIndex = 0; // index to iterate over

    // check for end condition, the 2nd '2' after a letter. "letter-bits ... 2 2"
    while (message_buf[currentIndex] != 2)
    {
        int charDecimal = 1; // to add leading 1 to binary value

        // decode each letter until the first 2 is reached.
        while (message_buf[currentIndex] != 2)
        {
            // decodes into decimal value of morse code w leading 1
            charDecimal = (charDecimal << 1) + message_buf[currentIndex];
            currentIndex++;
        }
        // currentIndex set to position after the end of a letter, AKA just after the '2' that marks the end of the letter-bits.
        currentIndex++;

        // translate and store the corresponding character into the character buffer
        char_message_buf[char_mess_buf_end] = getLetterMorseCode(charDecimal);
        char_mess_buf_end++;

        ESP_DRAM_LOGI(MORSE_TAG, "Character decoded: %c", getLetterMorseCode(charDecimal));
        // ESP_DRAM_LOGI(MORSE_TAG, "value at buffer start index :%d", message_buf[startIndex]); // what int is at the start of the next loop
    }
}

static void IRAM_ATTR gpio_start_event_handler(void *arg)
{
    // ignore false readings. Wait long enough for at least debounce delay.
    // and never allow for writing beyond the message buffer size, leaving room for the transmission end condition "2 2"
    if (((esp_timer_get_time() - start_time) < DEBOUNCE_DELAY) || input_in_progress || (mess_buf_end >= MESS_BUFFER_LENGTH - 2))
    {
        return;
    }
    input_in_progress = 1; // to prevent multipress

    start_time = esp_timer_get_time(); // store time of last event

    if ((start_time - time_last_end_event > SPACE_LENGTH) && (mess_buf_end != 0))
    {
        message_buf[mess_buf_end] = 2;
        mess_buf_end++;
        ESP_DRAM_LOGI(MORSE_TAG, "2 placed in buffer in start event");
    }
}

static void IRAM_ATTR gpio_end_event_handler(void *arg)
{
    // ignore false readings. Wait long enough for at least debounce delay. Ensure this is called only after a valid start press.
    if (((esp_timer_get_time() - time_last_end_event) < DEBOUNCE_DELAY) || !input_in_progress)
    {
        return;
    }

    time_last_end_event = esp_timer_get_time(); // store time of last event

    // ESP_DRAM_LOGI(MORSE_TAG, "last end time: %d", time_last_end_event);
    // ESP_DRAM_LOGI(MORSE_TAG, "time elapsed: %d", time_last_end_event - start_time);

    if (PRESS_LENGTH < (time_last_end_event - start_time))
    {
        // must hold button for at least press_length to get a 1
        message_buf[mess_buf_end] = 1;
        mess_buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "2 in buffer");
    }
    else
    {
        // 0 if button held for less than press_length time
        message_buf[mess_buf_end] = 0;
        mess_buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "1 in buffer");
    }

    input_in_progress = 0;

    ESP_DRAM_LOGW(MORSE_TAG, "placed in buffer: %d", message_buf[mess_buf_end - 1]);
}

static void IRAM_ATTR gpio_send_event_handler(void *arg)
{
    static int64_t lMillis = 0; // time since last send.
    uint8_t i;

    // ignore false readings. Wait at least 20ms before sending again.
    if (((esp_timer_get_time() - lMillis) < DEBOUNCE_DELAY) || input_in_progress)
        return;

    lMillis = esp_timer_get_time();

    // end each message with 2 twos
    if (mess_buf_end != 0)
    {
        message_buf[mess_buf_end++] = 2;
        message_buf[mess_buf_end] = 2;

        encodeMorseCode();

        ESP_DRAM_LOGI(MORSE_TAG, "character buffer end: %d", char_mess_buf_end);

        for (i = 0; i < char_mess_buf_end; i++)
        {
            ESP_DRAM_LOGI(MORSE_TAG, "character buffer[%d]: %c", i, char_message_buf[i]);
        }
    }

    char_mess_buf_end = 0;
    mess_buf_end = 0;
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

    profile_ptr->characteristic[characteristic_count] = *chr; // save the latest characteristic data

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
    // server_service_ptr = profile_ptr->service; // globally save the service information to server_service_ptr.

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

    // ble_uuid16_t *read_uuid;
    // read_uuid = malloc(sizeof(ble_uuid16_t));

    // read_uuid->u.type = BLE_UUID_TYPE_16;
    // read_uuid->value = READ_UUID;

    //  ble_uuid16_t *write_uuid;
    // write_uuid = malloc(sizeof(ble_uuid16_t));

    // write_uuid->u.type = BLE_UUID_TYPE_16;
    // write_uuid->value = WRITE_UUID;

    // ESP_LOGI(MORSE_TAG, "before disc by uuid");

    // err = ble_gattc_disc_chrs_by_uuid(conn_handle, service->start_handle, service->end_handle, &read_uuid->u, ble_gatt_chr_cb, profile_ptr);
    // if (err != 0)
    // {
    //     ESP_LOGI(ERROR_TAG, "ble_gattc_disc READUUID: error has occured: %u", err);
    //     return err;
    // }

    // err = ble_gattc_disc_chrs_by_uuid(conn_handle, service->start_handle, service->end_handle, &write_uuid->u, ble_gatt_chr_cb, profile_ptr);
    // if (err != 0)
    // {
    //     ESP_LOGI(ERROR_TAG, "ble_gattc_disc READUUID: error has occured: %u", err);
    //     return err;
    // }

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

    gpio_isr_handler_add(GPIO_INPUT_IO_SEND, gpio_send_event_handler, (void *)GPIO_INPUT_IO_SEND);
}

void host_task(void *param)
{
    nimble_port_run();
    // just ticks tbh
    int cnt = 0;
    while (1)
    {
        printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    return;
}

void ble_app_on_sync(void)
{
    // create the profile structure, allocate memory, and pass it the characteristic data
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
    err = ble_hs_id_set_rnd(client_ptr->val);
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set random address failed %d", err);
    }

    err = ble_gap_wl_set(server_ptr, white_list_count); // sets white list for connection to other device
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set whitelist failed");
    }

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

    // starts first task
    nimble_port_freertos_init(host_task);
}

void app_main(void)
{
    // gpio_setup();
    ble_client_setup();
}
