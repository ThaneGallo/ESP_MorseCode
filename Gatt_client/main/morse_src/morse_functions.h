#ifndef MORSE_FUNCTIONS_H
#define MORSE_FUNCTIONS_H

#include "morse_common.h"

#define PRESS_LENGTH 1000000 // Hold-time required for dash '-' input. 1 second in microseconds
#define SPACE_LENGTH 2000000 // Time required between inputs for new character start. 2 seconds in microseconds
#define DEBOUNCE_DELAY 500000 // time required between consecutive inputs to prevent debounce issues

// START, END, AND SEND EVENTS
#define GPIO_INPUT_IO_START 4 // start event sense
#define GPIO_INPUT_IO_END 5   // end event sense
#define GPIO_INPUT_IO_SEND 23 // send event sense
#define ESP_INTR_FLAG_DEFAULT 0

// the message and character buffers
#define MESS_BUFFER_LENGTH 2048
#define CHAR_BUFFER_LENGTH 256
extern uint8_t message_buf[MESS_BUFFER_LENGTH];
extern char char_message_buf[CHAR_BUFFER_LENGTH];
extern uint32_t mess_buf_end;
extern uint32_t char_mess_buf_end;

/**
 * Prints contents of message buffer and character message buffer
 */
void debug_print_buffer();

/**
 * Converts decimal value of Morse code to char.
 * @param decimalValue the decimal interpretation of the morse input. Example 'a' = .- = 5.
 *  Note that there is a leading 1 on the binary input of decimalValue.
 * @return the character corresponding to the morse code decimalValue.
 */
char get_letter_morse_code(int decimalValue);

/**
 * Converts message_buf values into corresponding characterBuffer values.
 */
void encode_morse_code();

/**
 * Handle the initial neg-edge push of a button for the morse_code translation.
 * Marks time to later translate 1 or 0.
 */
void IRAM_ATTR gpio_start_event_handler(void *arg);

/**
 * Handle the pos-edge after-effect of a button for the morse_code translation.
 * Marks end time and takes previous time to decide if 1 or 0 and places input into buffer.
 */
void IRAM_ATTR gpio_end_event_handler(void *arg);

/**
 * Handles the operation when data is sent to server.
 */
void IRAM_ATTR gpio_send_event_handler(void *arg);

/**
 * Handles the operation where data is read from server.
 */
void IRAM_ATTR gpio_read_event_handler(void *arg);

#endif