#ifndef POLL_EVENT_TASK_FUNCTIONS_H
#define POLL_EVENT_TASK_FUNCTIONS_H

#include "morse_common.h"

#define POLL_EVENT_READ_FLAG 1
#define POLL_EVENT_SEND_FLAG 2

/**
 * Sets all flags to the value given.
 */
void poll_event_set_all_flags(bool val);

/**
 * Set a particular flag based on the following key.
 * @param flag the flag to set. 1 == read_flag, 2 == send_flag.
 * @param val the boolean to set the flag to. true or false.
 * @return 0 on success, -1 on error.
 */
int poll_event_set_flag(uint8_t flag, bool val);

/**
 * Checks for any event by monitoring certain flags. If any flag is true, then triggers correct task.
 */
void poll_event_task(void *param);

// COMMENTED BECAUSE MORSE_COMMON.C NOW GLOBALLY INITIALIZES THE BLE_PROFILE
// /**
//  * Sets the profile pointer internally for other functions to use.
//  * @return 0 on success, !0 on failure.
//  */
// int poll_event_set_profile_ptr(ble_profile *profile_ptr);




#endif