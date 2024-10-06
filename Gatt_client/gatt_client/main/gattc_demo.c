/*modified 10/6/2024*/

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

static struct ble_profile
{
    const struct ble_gap_conn_desc *conn_desc;
    const struct ble_gatt_svc *service;
    // struct ble_gatt_chr characteristic[CHARACTERISTIC_ARR_MAX]; // characteristic array holds all the characteristics.
    struct ble_gatt_chr **characteristic; // characteristic array holds all the characteristics.
};

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
#define SERVICE_UUID 0xCAFE
#define READ_UUID 0xCAFF
#define WRITE_UUID 0xDECA

// static uint8_t write_val = WRITE_UUID;
// static uint8_t service_val = SERVICE_UUID;

// forward declarations
static int ble_gatt_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);

/**
 * Converts decimal value of Morse code to char.
 * @param decimalValue the decimal interpretation of the morse input. Example 'a' = .- = 5.
 *  Note that there is a leading 1 on the binary input of decimalValue.
 * @return the character corresponding to the morse code decimalValue.
 */
char getLetterMorseCode(int decimalValue)
{
    /*
    decimalValue has a leading 1 to determine the start of the morse input.
        for example, A: .- , would directly translate to just 01, but to remove
        issues of .- being different from ..-, we have added in a leading 1.
    Hence, A: .- = 101 = 5.
    */
    switch (decimalValue)
    {
    case 5:
        // Handle case for A: .-
        return 'a';
    case 24:
        // Handle case for B: -..
        return 'b';
    case 26:
        // Handle case for C: -.-.
        return 'c';
    case 12:
        // Handle case for D: -..
        return 'd';
    case 2:
        // Handle case for E: .
        return 'e';
    case 18:
        // Handle case for F: ..-.
        return 'f';
    case 14:
        // Handle case for G: --.
        return 'g';
    case 16:
        // Handle case for H: ....
        return 'h';
    case 4:
        // Handle case for I: ..
        return 'i';
    case 23:
        // Handle case for J: .---
        return 'j';
    case 13:
        // Handle case for K: -.-
        return 'k';
    case 20:
        // Handle case for L: .-..
        return 'l';
    case 7:
        // Handle case for M: --
        return 'm';
    case 6:
        // Handle case for N: -.
        return 'n';
    case 15:
        // Handle case for O: ---
        return 'o';
    case 22:
        // Handle case for P: .--.
        return 'p';
    case 29:
        // Handle case for Q: --.-
        return 'q';
    case 10:
        // Handle case for R: .-.
        return 'r';
    case 8:
        // Handle case for S: ...
        return 's';
    case 3:
        // Handle case for T: -
        return 't';
    case 9:
        // Handle case for U: ..-
        return 'u';
    case 17:
        // Handle case for V: ...-
        return 'v';
    case 11:
        // Handle case for W: .--
        return 'w';
    case 25:
        // Handle case for X: -..-
        return 'x';
    case 27:
        // Handle case for Y: -.--
        return 'y';
    case 28:
        // Handle case for Z: --..
        return 'z';
    case 63:
        // Handle case for 0: -----
        return '0';
    case 47:
        // Handle case for 1: .----
        return '1';
    case 39:
        // Handle case for 2: ..---
        return '2';
    case 35:
        // Handle case for 3: ...--
        return '3';
    case 33:
        // Handle case for 4: ....-
        return '4';
    case 64:
        // Handle case for 5: .....
        return '5';
    case 48:
        // Handle case for 6: -....
        return '6';
    case 56:
        // Handle case for 7: --...
        return '7';
    case 60:
        // Handle case for 8: ---..
        return '8';
    case 61:
        // Handle case for 9: ----.
        return '9';
    default:
        // Handle unknown cases
        return '=';
    }
}

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

    // check if there is an error
    if (error->status != 0)
    {
        ESP_LOGI(ERROR_TAG, "ble_gatt_disc_svc: an error has occured: error %u -> %u", error->att_handle, error->status);
        return error->status;
    }

    profile_ptr->characteristic[characteristic_count] = chr; // save the latest characteristic data

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

    struct ble_profile *profile_ptr = (struct ble_profile *)arg;
    uint8_t err = 0;

    if (profile_ptr == NULL)
    {
        ESP_LOGI(MORSE_TAG, "profile ptr is null");
        return -1;
    }

    // check if there is an error
    if (error->status != 0)
    {
        ESP_LOGI(ERROR_TAG, "ble_gatt_disc_svc: an error has occured: attr handle:%u status -> %u", error->att_handle, error->status);
        return error->status;
    }
    // server_service_ptr = profile_ptr->service; // globally save the service information to server_service_ptr.

    // ESP_LOGI(MORSE_TAG, "before service alloc");

    profile_ptr->service = service;

    //  ESP_LOGI(DEBUG_TAG, "", rc);

    // ESP_LOGI(MORSE_TAG, "after service alloc");

    // discover all characteristics
    err = ble_gattc_disc_all_chrs(conn_handle, profile_ptr->service->start_handle, profile_ptr->service->end_handle, ble_gatt_chr_cb, profile_ptr);
    if (err != 0)
    {
        ESP_LOGI(ERROR_TAG, "ble_gattc_disc_all_chrs: an error has occured: %u", err);
        return err;
    }

    ble_uuid16_t *read_uuid;
    read_uuid = malloc(sizeof(ble_uuid16_t));

    read_uuid->u.type = BLE_UUID_TYPE_16;
    read_uuid->value = READ_UUID;

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

    err = ble_gattc_disc_all_svcs(profile->conn_desc->conn_handle, ble_gatt_disc_svc_cb, &profile); // discover all primary services
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

//     // literally what the fuck is the struct supposed to be
//     static void service_cb(uint16_t conn_handle,
//                            const struct ble_gatt_error *error,
//                            const struct ble_gatt_chr *chr, void *arg)
// {
//     switch (event->type)
//     {
//     case BLE_GATTC_EVENT_READ_RSP:
//         ESP_LOGI(MORSE_TAG, "GATTC Read");
//         break;

//     case BLE_GATTC_EVENT_WRITE_RSP:
//         ESP_LOGI(MORSE_TAG, "GATTC Write");
//         break;

//     default:
//         ESP_LOGI(MORSE_TAG, "Called Event without handler: %u", event->type);
//         break;
//     }
// }

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
    uint8_t err;

    // struct ble_gap_conn_desc *conn_desc; // create memory for the conn_desc
    // struct ble_gatt_ *conn_desc;
    // struct ble_gap_conn_desc *conn_desc;

    // struct ble_gap_conn_desc* conn_desc; // Pointer to it
    // struct ble_gatt_svc* service;
    // //struct ble_gatt_chr characteristic[CHARACTERISTIC_ARR_MAX] = {{0}}; // initialize characteristic array (size 2) with all 0's.

    struct ble_gatt_chr **characteristic;

    characteristic = malloc(sizeof(struct ble_gatt_chr *) * CHARACTERISTIC_ARR_MAX);
    if (!characteristic)
    {
        ESP_LOGI(ERROR_TAG, "BLE Characteristic is NULL on line %d", __LINE__);
        free(characteristic);
    }

    for (int i = 0; i < CHARACTERISTIC_ARR_MAX; i++)
    {
        characteristic[i] = malloc(sizeof(struct ble_gatt_chr) * CHARACTERISTIC_ARR_MAX);
        if (characteristic[i] == NULL)
        {
            ESP_LOGI(ERROR_TAG, "BLE Characteristic is NULL on line %d", __LINE__);
            free(characteristic[i]);
        }

        struct ble_profile *profile;

        profile = malloc(sizeof(struct ble_profile));

        profile->conn_desc = malloc(sizeof(struct ble_gap_conn_desc));
        profile->service = malloc(sizeof(struct ble_gatt_svc));
        profile->characteristic = characteristic;

        // profile.conn_desc = &server_desc; // tell the profile->conn_desc where to point to in actual memory
        // profile.service = service;
        // profile.characteristic = characteristic;

        // struct ble_profile profile = {
        //     .conn_desc = &conn_desc,
        //     .service = &service,
        //     .characteristic = characteristic

        // }; // profile creation in memory

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
