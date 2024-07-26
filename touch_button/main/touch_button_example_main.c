/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static int messageBuffer[10];
static uint8_t buf_end = 0;

static uint64_t start_time;
static uint64_t end_time;


static QueueHandle_t gpio_evt_queue = NULL;

#define GPIO_INPUT_IO_0 4
#define GPIO_INPUT_IO_1 5
#define GPIO_INPUT_IO_2 23

#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0
#define MORSE_TAG "Morse code tag"

static void IRAM_ATTR gpio_start_event_handler(void *arg)
{
   
}

static void IRAM_ATTR gpio_end_event_handler(void *arg)
{
   
}



static void IRAM_ATTR gpio_fill_buffer(void *arg)
{
    // int check1 = 0;

    // check1 = gpio_get_level(GPIO_INPUT_IO_0);

    // // bc button pulls low
    // if (!check1)
    // {
    //     messageBuffer[buf_end] = 1;
    //     buf_end++;
    // }
    // else
    // {
    //     messageBuffer[buf_end] = 0;
    //     buf_end++;
    // }

    ESP_DRAM_LOGI(MORSE_TAG, "inside fill buffer");
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
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
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

    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_fill_buffer, (void *)GPIO_INPUT_IO_0);

    // hook isr handler for specific gpio pin
    // only need one to send buffer
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_send_buffer, (void *)GPIO_INPUT_IO_1);

   

    int cnt = 0;
    while (1)
    {
        printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
