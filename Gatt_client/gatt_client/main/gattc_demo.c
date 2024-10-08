﻿/*modified 9/4/2024*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_bt.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// gpio
static uint32_t buf_end = 0;
static uint32_t charBufEnd = 0;

static int64_t start_time;
static int64_t end_time;
static int64_t time_last_end_event;
static bool input_in_progress;

#define PRESS_LENGTH 1000000 // 1 second in microseconds
#define SPACE_LENGTH 2000000 // 2 seconds in microseconds

static QueueHandle_t gpio_evt_queue = NULL;

// START, END, AND SEND EVENTS
#define GPIO_INPUT_IO_START 4 // start event sense
#define GPIO_INPUT_IO_END 5   // end event sense
#define GPIO_INPUT_IO_SEND 23 // send event sense

#define BUFFER_LENGTH 2048
static uint8_t messageBuffer[2048];
static char charMessageBuffer[255];

#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_START) | (1ULL << GPIO_INPUT_IO_END))
#define ESP_INTR_FLAG_DEFAULT 0
#define MORSE_TAG "Morse code tag"
#define DEBOUNCE_DELAY 50000 // time required between consecutive inputs to prevent debounce issues

// ble address structs and pointers
static const ble_addr_t serverAddr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xDE, 0xCA, 0xFB, 0xEE, 0xFE, 0xD0}};

static const ble_addr_t clientAddr = {
    .type = BLE_ADDR_RANDOM, // Example type value
    .val = {0xCA, 0xFF, 0xED, 0xBE, 0xEE, 0xEF}};

const uint8_t *clientAddressVal = &clientAddr.val;
uint8_t ble_addr_type;

// static const ble_addr_t *serverPtr = &serverAddr;
// static const ble_addr_t *clientPtr = &clientAddr;

static struct ble_gap_conn_desc *clientDesc = NULL;

// UUID macros
#define SERVICE_UUID 0xCAFE
#define READ_UUID 0xCAFF
#define WRITE_UUID 0xDECA

// converts decimal value of Morse code to char
char getLetterMorseCode(int decimalValue)
{
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

void debugPrintBuffer()
{
    int i;

    for (i = 0; i <= buf_end; i++)
    {
        ESP_DRAM_LOGI(MORSE_TAG, "message buffer[%d]: %d", i, messageBuffer[i]);
    }

    for (i = 0; i <= charBufEnd; i++)
    {
        ESP_DRAM_LOGI(MORSE_TAG, "character buffer[%d]: %c", i, charMessageBuffer[i]);
    }
}

void encodeMorseCode()
{
    int startIndex = 0;  // index of last 2
    int charDecimal = 1; // to add leading 1 to binary value

    int i = 0;

    do // checks for end condition "2 2"
    {

        do // decode each letter until the first 2 is reached
        {
            // decodes into decimal value of morse code w leading 1
            charDecimal = (charDecimal << 1) + messageBuffer[i + startIndex];

            // ESP_DRAM_LOGI(MORSE_TAG, "Character decimal: %d loop# %d", charDecimal, i);
            // ESP_DRAM_LOGI(MORSE_TAG, "index: %d", i + startIndex);

            i++;

        } while (messageBuffer[i + startIndex] != 2);

        // // sets inex of 2 value found to the start index for the next letter
        // ESP_DRAM_LOGI(MORSE_TAG, "Character decimal: %d", charDecimal);

        startIndex = startIndex + i + 1;

        charMessageBuffer[charBufEnd] = getLetterMorseCode(charDecimal);

        ESP_DRAM_LOGI(MORSE_TAG, "Character decoded: %c", getLetterMorseCode(charDecimal));

        // set to zero for next letter
        i = 0;
        charDecimal = 1;
        charBufEnd++;

        // ESP_DRAM_LOGI(MORSE_TAG, "value at buffer start index :%d", messageBuffer[startIndex]);

    } while (messageBuffer[startIndex] != 2);
}

static void IRAM_ATTR gpio_start_event_handler(void *arg)
{
    // ignore false readings. Wait at least 20ms.
    if (((esp_timer_get_time() - start_time) < DEBOUNCE_DELAY) || input_in_progress)
    {
        return;
    }

    input_in_progress = 1; // to prevent multipress

    start_time = esp_timer_get_time(); // store time of last event

    if ((start_time - time_last_end_event > SPACE_LENGTH) && (buf_end != 0))
    {
        messageBuffer[buf_end] = 2;
        buf_end++;
        ESP_DRAM_LOGI(MORSE_TAG, "2 placed in buffer in start event");
    }
}

static void IRAM_ATTR gpio_end_event_handler(void *arg)
{
    // ignore false readings. Wait at least 20ms.
    if ((esp_timer_get_time() - end_time) < DEBOUNCE_DELAY)
    {
        return;
    }

    end_time = esp_timer_get_time(); // store time of last event
    time_last_end_event = end_time;

    // ESP_DRAM_LOGI(MORSE_TAG, "end time: %d", end_time);

    // ESP_DRAM_LOGI(MORSE_TAG, "time elapsed: %d", end_time - start_time);

    // handles out of bounds "errors"
    if (buf_end >= BUFFER_LENGTH - 2)
    {
        return;
    }

    if (PRESS_LENGTH < (end_time - start_time))
    {
        // must hold button for at least press_length to get a 1
        messageBuffer[buf_end] = 1;
        buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "2 in buffer");
    }
    else
    {
        // 0 if button held for less than press_length time
        messageBuffer[buf_end] = 0;
        buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "1 in buffer");
    }

    input_in_progress = 0;

    ESP_DRAM_LOGW(MORSE_TAG, "placed in buffer: %d", messageBuffer[buf_end - 1]);
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
    if ((buf_end != 0) && (!input_in_progress))
    {
        messageBuffer[buf_end++] = 2;
        messageBuffer[buf_end] = 2;

        encodeMorseCode();

        ESP_DRAM_LOGI(MORSE_TAG, "character buffer end: %d", charBufEnd);

        for (i = 0; i < charBufEnd; i++)
        {
            ESP_DRAM_LOGI(MORSE_TAG, "character buffer[%d]: %c", i, charMessageBuffer[i]);
        }
    }

    charBufEnd = 0;
    buf_end = 0;
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;

    printf("in gpio task");

    for (;;)
    {                                                              // ; = infinite loop
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) // if the queue is recieved a task
        {
            printf("GPIO[%" PRIu32 "] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

static int scan_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        // Handle device discovery
        ESP_LOGI(MORSE_TAG, "Device found: %x", event->disc.addr.val[0]);
        // Connect to the device if it matches your criteria
        // Replace `event->disc.addr` with the address of the device you want to connect to
        ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &serverAddr, 10000, NULL, NULL, NULL);
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
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_START) | (1ULL << GPIO_INPUT_IO_SEND);
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable high on default
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_END);
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable high on default
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // makes event queue
    // create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

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
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically

    // err = ble_gap_wl_set(clientPtr, white_list_count); // sets white list for connection to other device
    // if (err != 0)
    // {
    //     ESP_LOGI(GATTS_TAG, "BLE gap set whitelist failed");
    // }

    // from cooper's video
    struct ble_gap_disc_params disc_params;
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    err = ble_gap_disc(ble_addr_type, 10000, &disc_params, scan_cb, NULL);
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "BLE GAP Discovery Failed");
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
    //  err = ble_hs_id_gen_rnd(staticAddress, );
    //  if(err != 0){
    //      ESP_LOGI(MORSE_TAG, "Random Address Creation Failed");
    //  }

    err = ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    if (err != 0)
    {
        ESP_LOGI(MORSE_TAG, "Address infer auto Failed");
    }

    // // sets rand client address to array above
    // err = ble_hs_id_set_rnd(clientAddressVal);
    // if (err != 0)
    // {
    //     ESP_LOGI(MORSE_TAG, "Random Address Set Failed");
    // }

    // ESP_LOGI(MORSE_TAG, "after ble_hs_set_rnd");

    // // doesnt connect to peer
    // err = ble_gap_conn_find_by_addr(&serverAddr, clientDesc);
    // if (err != 0)
    // {
    //     ESP_LOGI(MORSE_TAG, "BLE Connection Find by Address Failed");
    // }

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
    // gpio_setup();
    ble_client_setup();
}
