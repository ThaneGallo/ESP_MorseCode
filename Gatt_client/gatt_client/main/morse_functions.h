#ifndef MORSE_FUNCTIONS_H
#define MORSE_FUNCTIONS_H


#define PRESS_LENGTH 1000000 // Hold-time required for dash '-' input. 1 second in microseconds
#define SPACE_LENGTH 2000000 // Time required between inputs for new character start. 2 seconds in microseconds
#define DEBOUNCE_DELAY 50000 // time required between consecutive inputs to prevent debounce issues

// START, END, AND SEND EVENTS
#define GPIO_INPUT_IO_START 4 // start event sense
#define GPIO_INPUT_IO_END 5   // end event sense
#define GPIO_INPUT_IO_SEND 23 // send event sense
#define ESP_INTR_FLAG_DEFAULT 0

#define MESS_BUFFER_LENGTH 2048
#define CHAR_BUFFER_LENGTH 256


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

static void IRAM_ATTR gpio_start_event_handler(void *arg);

static void IRAM_ATTR gpio_end_event_handler(void *arg);

static void IRAM_ATTR gpio_send_event_handler(void *arg);

static void IRAM_ATTR gpio_read_event_handler(void *arg);



#endif