#include "morse_functions.h"
#include "poll_event_task_functions.h"

// debounce macro
#define DEBOUNCE_MILLIS(x) static int64_t lMillis = 0; if((esp_timer_get_time() - lMillis) < x) return; lMillis = esp_timer_get_time();

int64_t start_time;          // time of last valid start
int64_t time_last_end_event; // time of last valid end
int64_t lMillis = 0; // time since last send.
bool input_in_progress;

// initialize the buffers
uint8_t message_buf[MESS_BUFFER_LENGTH];
char char_message_buf[CHAR_BUFFER_LENGTH];
uint32_t mess_buf_end = 0;
uint32_t char_mess_buf_end = 0;

void debug_print_buffer()
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

char get_letter_morse_code(int decimalValue)
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

void encode_morse_code()
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
        char_message_buf[char_mess_buf_end] = get_letter_morse_code(charDecimal);
        char_mess_buf_end++;

        // ESP_DRAM_LOGI(MORSE_TAG, "Character decoded: %c", get_letter_morse_code(charDecimal));
        // ESP_DRAM_LOGI(MORSE_TAG, "value at buffer start index :%d", message_buf[startIndex]); // what int is at the start of the next loop
    }
}

void IRAM_ATTR gpio_start_event_handler(void *arg)
{
    // ignore false readings. Wait long enough for at least debounce delay.
    // and never allow for writing beyond the message buffer size, leaving room for the transmission end condition "2 2"
    if (((esp_timer_get_time() - start_time) < DEBOUNCE_DELAY) || input_in_progress || (mess_buf_end >= MESS_BUFFER_LENGTH - 2))
    {
        return;
    }
    input_in_progress = 1; // to prevent multipress
    start_time = esp_timer_get_time(); // store time of last event
    // // CHECK FOR DEBOUNCE
    // DEBOUNCE_MILLIS(DEBOUNCE_DELAY);

    if ((start_time - time_last_end_event > SPACE_LENGTH) && (mess_buf_end != 0))
    {
        message_buf[mess_buf_end] = 2;
        mess_buf_end++;
        ESP_DRAM_LOGI(MORSE_TAG, "2 placed in buffer in start event");
    }
}

void IRAM_ATTR gpio_end_event_handler(void *arg)
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

void IRAM_ATTR gpio_send_event_handler(void *arg)
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

        encode_morse_code();

        ESP_DRAM_LOGI(MORSE_TAG, "character buffer end: %d", char_mess_buf_end);

        for (i = 0; i < char_mess_buf_end; i++)
        {
            ESP_DRAM_LOGI(MORSE_TAG, "character buffer[%d]: %c", i, char_message_buf[i]);
        }
    }

    poll_event_set_all_flags(true); // set write and read checks to true.
    // char_mess_buf_end = 0;
    // mess_buf_end = 0;
}

/**
 * When read button pressed, read the attribute from the server.
 * Always pass in the profile_ptr as a void argument.
 */
void IRAM_ATTR gpio_read_event_handler(void *arg) {
    
    static int64_t lMillis = 0; 
    if((esp_timer_get_time() - lMillis) < DEBOUNCE_DELAY || input_in_progress) {
        return; 
    }
    // ESP_DRAM_LOGI(DEBUG_TAG, "time = %d", esp_timer_get_time());
    // ESP_DRAM_LOGI(DEBUG_TAG, "lmillis = %d", lMillis);
    // ESP_DRAM_LOGI(DEBUG_TAG, "difference = %d", esp_timer_get_time() - lMillis);
    lMillis = esp_timer_get_time();

    ESP_DRAM_LOGI(DEBUG_TAG, "start of read_event");
    if(!ble_profile1) {
        ESP_DRAM_LOGI(DEBUG_TAG, "ble_profile is read_event is NULL");
        return;
    }

    ESP_DRAM_LOGI(DEBUG_TAG, "[conn_handle, val_handle] = [%d, %d]", ble_profile1->conn_desc->conn_handle, ble_profile1->characteristic->val_handle);
    int rc = poll_event_set_flag(POLL_EVENT_READ_FLAG, true);
    // int rc = ble_gattc_read(ble_profile1->conn_desc->conn_handle, ble_profile1->characteristic->val_handle, ble_gatt_read_chr_cb, arg);
    if(rc != 0) {
        ESP_DRAM_LOGI(ERROR_TAG, "read_event error rc = %d", rc);
        return;
    }
}
