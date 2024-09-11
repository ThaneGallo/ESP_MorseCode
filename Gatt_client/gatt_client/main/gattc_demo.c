/*modified 9/9/2024*/

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
static uint32_t charBufEnd = 0;

static int64_t start_time; // time of last valid start 
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
static uint8_t messageBuffer[MESS_BUFFER_LENGTH];
static char charMessageBuffer[CHAR_BUFFER_LENGTH];

#define ESP_INTR_FLAG_DEFAULT 0
#define MORSE_TAG "Morse code tag"
#define DEBOUNCE_DELAY 50000 // time required between consecutive inputs to prevent debounce issues

// ble address structs and pointers
static const ble_addr_t serverAddr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xDE, 0xCA, 0xFB, 0xEE, 0xFE, 0xD2}
    //.val = {0xD0, 0xFE, 0xEE, 0xFB, 0xCA, 0xDE}
};

static const ble_addr_t clientAddr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xCA, 0xFF, 0xED, 0xBE, 0xEE, 0xEF}
    //.val = {0xEF, 0xEE, 0xBE, 0xED, 0xFF, 0xCA}
};

static uint8_t ble_addr_type = BLE_OWN_ADDR_RANDOM;
//const uint8_t *clientAddressVal = &clientAddr.val;
static const ble_addr_t *serverPtr = &serverAddr;
static const ble_addr_t *clientPtr = &clientAddr;

static struct ble_gap_conn_desc *clientDesc = NULL;

// DISCOVERY PARAMETERS FOR SEARCH
static struct ble_gap_disc_params disc_params = {
    .filter_duplicates = 1,
    .passive = 0,
    .itvl = 0,
    .window = 0,
    .filter_policy = BLE_HCI_SCAN_FILT_USE_WL_INITA,
    //.filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
    .limited = 0
};

// UUID macros
#define SERVICE_UUID 0xCAFE
#define READ_UUID 0xCAFF
#define WRITE_UUID 0xDECA

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
        ESP_DRAM_LOGI(MORSE_TAG, "message buffer[%d]: %d", i, messageBuffer[i]);
    }

    for (i = 0; i <= charBufEnd; i++)
    {
        ESP_DRAM_LOGI(MORSE_TAG, "character buffer[%d]: %c", i, charMessageBuffer[i]);
    }
}

/**
 * Converts messageBuffer values into corresponding characterBuffer values.
 */
void encodeMorseCode()
{
    /*
    - For each bit-letter-combo in the messageBuffer array, 
    - grab the bits for each letter individually, stopping when you reach the '2' at the end of the input
    - translate that into the corresponding character using getLetterMorseCode()
    - save that character into the character buffer
    */

    int currentIndex = 0;  // index to iterate over

    // check for end condition, the 2nd '2' after a letter. "letter-bits ... 2 2"
    while (messageBuffer[currentIndex] != 2) {
        int charDecimal = 1; // to add leading 1 to binary value

        // decode each letter until the first 2 is reached.
        while (messageBuffer[currentIndex] != 2) {
            // decodes into decimal value of morse code w leading 1
            charDecimal = (charDecimal << 1) + messageBuffer[currentIndex];
            currentIndex++;
        }
        // currentIndex set to position after the end of a letter, AKA just after the '2' that marks the end of the letter-bits.
        currentIndex++;
        
        // translate and store the corresponding character into the character buffer
        charMessageBuffer[charBufEnd] = getLetterMorseCode(charDecimal);
        charBufEnd++;

        ESP_DRAM_LOGI(MORSE_TAG, "Character decoded: %c", getLetterMorseCode(charDecimal));
        // ESP_DRAM_LOGI(MORSE_TAG, "value at buffer start index :%d", messageBuffer[startIndex]); // what int is at the start of the next loop
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
        messageBuffer[mess_buf_end] = 2;
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
        messageBuffer[mess_buf_end] = 1;
        mess_buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "2 in buffer");
    }
    else
    {
        // 0 if button held for less than press_length time
        messageBuffer[mess_buf_end] = 0;
        mess_buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "1 in buffer");
    }

    input_in_progress = 0;

    ESP_DRAM_LOGW(MORSE_TAG, "placed in buffer: %d", messageBuffer[mess_buf_end - 1]);
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
        messageBuffer[mess_buf_end++] = 2;
        messageBuffer[mess_buf_end] = 2;

        encodeMorseCode();

        ESP_DRAM_LOGI(MORSE_TAG, "character buffer end: %d", charBufEnd);

        for (i = 0; i < charBufEnd; i++)
        {
            ESP_DRAM_LOGI(MORSE_TAG, "character buffer[%d]: %c", i, charMessageBuffer[i]);
        }
    }

    charBufEnd = 0;
    mess_buf_end = 0;
}

static int scan_cb(struct ble_gap_event *event, void *arg)
{
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
        
        // err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, serverPtr, 10000, NULL, NULL, NULL);
        err = ble_gap_connect(BLE_OWN_ADDR_RANDOM, &event->disc.addr, 10000, NULL, NULL, NULL);
        switch (err) {
            case 0:
                ESP_LOGI(MORSE_TAG, "ble_gap_connect successful");
                err = ble_gap_conn_find_by_addr(&event->disc.addr, clientDesc); // setup clientDesc with the data
                if (err != 0)
                {
                    ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address Failed");
                }
                ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address successful");
                err = ble_gap_terminate(clientDesc->conn_handle, BLE_ERR_CONN_SPVN_TMO);
                if (err != 0)
                {
                    if (err == BLE_HS_ENOTCONN) {
                        ESP_LOGI(MORSE_TAG, "BLE Connection no connection within specified handle");
                    }
                    ESP_LOGI(MORSE_TAG, "BLE Connection terminate failed, error code: %d", err);
                }
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

/**
 * Helper method to activate gap discovery events, without needing to remember the details each time.
 * @return uint8_t, the error code or success code of ble_gap_disc.
 */
uint8_t activate_gap_discovery() {
    return ble_gap_disc(ble_addr_type, 10 * 1000, &disc_params, scan_cb, NULL);
}

void ble_app_on_sync(void)
{
    uint8_t err;
    //ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically

    err = ble_hs_id_set_rnd(clientPtr->val); 
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set random address failed %d", err);
    }

    ESP_LOGI(MORSE_TAG, "after ble_hs_set_rnd");

    err = ble_gap_wl_set(serverPtr, white_list_count); // sets white list for connection to other device
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE gap set whitelist failed");
    }

    ESP_LOGI(MORSE_TAG, "after ble_gap_wl_set");

    // use the discovery macro
    err = activate_gap_discovery();
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE GAP Discovery Failed: %u", err);
    }

    ESP_LOGI(MORSE_TAG, "after ble_gap_disc");

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

    ble_svc_gap_init();
   
    ble_hs_cfg.sync_cb = ble_app_on_sync; 


    ESP_LOGI(MORSE_TAG, "after init s");

    // creates and sets random address
    // err = ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    // if (err != 0)
    // {
    //     ESP_LOGI(MORSE_TAG, "Address infer auto Failed");
    // }

    // // ____________________________________________________________________________________
    // // doesnt connect to peer
    // err = ble_gap_conn_find_by_addr(serverPtr, clientDesc);
    // if (err != 0)
    // {
    //     ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address Failed");
    // }
    // // ____________________________________________________________________________________

    // ESP_LOGI(MORSE_TAG, "after ble_gap_conn_find_addr");

    // BLE_HS_FOREVER

    // when completed BLE_GAP_EVENT_DISC_COMPLETE is triggered

    // // discovers primary service by uuid does it need to be discovered?
    // err = ble_gattc_disc_svc_by_uuid(clientDesc->conn_handle, SERVICE_UUID, service_cb, NULL);
    // if (err != 0)
    // {
    //     ESP_LOGI(MORSE_TAG, "Discover Service Failed");
    // }

    // ble_gatts_find_svc(const ble_uuid_t *uuid, uint16_t *out_handle);

    // // ble_gatt_svc contains start and end handle

    // // forgot where start and end handle exist ;(
    // ble_gattc_disc_chrs_by_uuid(clientDesc->conn_handle, uint16_t start_handle, uint16_t end_handle, WRITE_UUID, ble_gatt_chr_fn * cb, void *cb_arg)

    // starts first task
    nimble_port_freertos_init(host_task);
}

void app_main(void)
{
    //gpio_setup();
    ble_client_setup();
}
