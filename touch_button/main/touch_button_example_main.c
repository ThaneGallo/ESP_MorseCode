/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_log.h"

static uint8_t messageBuffer[10];
static uint8_t buffer_location = 0;

static QueueHandle_t gpio_evt_queue = NULL;

#define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1     CONFIG_GPIO_INPUT_1
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0


static void gpio_send_buffer(void* arg)
{

}


void app_main(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};

    //interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable high on default
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);


    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_fill_buffer, (void*) GPIO_INPUT_IO_0);


    //hook isr handler for specific gpio pin
    //only need one to send buffer
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_send_buffer, (void*) GPIO_INPUT_IO_1);


    // start buffer reading text with element read until long wait period (5 sec) of no readings
    // second button to send message (1 to type the other to encode/decode)
    // spit out final bufer / decoded text

// long vs short 

while(1){
    printf("tick");


}


    
}
