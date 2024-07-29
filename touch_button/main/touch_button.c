/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "esp_log.h"

static int messageBuffer[10];
static uint8_t buf_end = 0;

static int64_t start_time;
static int64_t end_time;
static int64_t press_length = 1000000; // 1 second in microseconds

static QueueHandle_t gpio_evt_queue = NULL;

// START, END, AND SEND EVENTS
#define GPIO_INPUT_IO_START 4 // start event sense
#define GPIO_INPUT_IO_END 5 // end event sense
#define GPIO_INPUT_IO_SEND 23 // send event sense
#define BUFFER_LENGTH 10

#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_START) | (1ULL << GPIO_INPUT_IO_END))
#define ESP_INTR_FLAG_DEFAULT 0
#define MORSE_TAG "Morse code tag"
#define DEBOUNCE_DELAY 20000 // time required between consecutive inputs to prevent debounce issues



//probably race condition between handlers for filling the buffer

static void IRAM_ATTR gpio_start_event_handler(void *arg)
{
    // ignore false readings. Wait at least 20ms.
    if ((esp_timer_get_time() - start_time) < DEBOUNCE_DELAY) {
        return;
    }

    start_time = esp_timer_get_time(); // store time of last event
    ESP_DRAM_LOGI(MORSE_TAG, "start time: %d", start_time);
}

static void IRAM_ATTR gpio_end_event_handler(void *arg)
{
    // ignore false readings. Wait at least 20ms.
    if ((esp_timer_get_time() - end_time) < DEBOUNCE_DELAY) {
        return;
    }

    end_time = esp_timer_get_time(); // store time of last event
    ESP_DRAM_LOGI(MORSE_TAG, "end time: %d", end_time);

    ESP_DRAM_LOGI(MORSE_TAG, "time elapsed: %d", end_time - start_time);

    //handles out of bounds "errors"
    if(buf_end >= BUFFER_LENGTH){
        return;
    }

    if (press_length < (end_time - start_time))
    {
        // must hold button for at least press_length to get a 1
        messageBuffer[buf_end] = 1;
        buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "1 in buffer");
    }
    else
    {
        // 0 if button held for less than press_length time
        messageBuffer[buf_end] = 0;
        buf_end++;
        // ESP_DRAM_LOGI(MORSE_TAG, "0 in buffer");
    }

    ESP_DRAM_LOGW(MORSE_TAG, "placed in buffer: %d", messageBuffer[buf_end - 1]);
}

static void IRAM_ATTR gpio_send_event_handler(void *arg)
{
    static int64_t lMillis = 0; // time since last send.
    uint8_t i;

    // ignore false readings. Wait at least 20ms before sending again.
    if ((esp_timer_get_time() - lMillis) < DEBOUNCE_DELAY)
        return;

    lMillis = esp_timer_get_time();

    for (i = 0; i < buf_end; i++) {
        ESP_DRAM_LOGI(MORSE_TAG, "buffer[%d]: %d", i, messageBuffer[i]);
    }

    buf_end = 0;

}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;

    printf("in gpio task");

    for (;;)
    {                                                              // ;; = infinite loop
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) // if the queue is recieved a task
        {
            printf("GPIO[%" PRIu32 "] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void app_main(void)
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

    int cnt = 0;
    while (1)
    {
        printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
