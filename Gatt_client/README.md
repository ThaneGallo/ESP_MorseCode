# Gatt_client

## Organization

Within the main directory, morse_client.c is our main file and all of the header files below are located within the morse_src directory.

By default the name of the client device is "BLE-Scan-Client".

### morse_common.c/h
Contains all files and variables which are to be global between all headers. The headers included are ESP NimBLE libraries, FreeRTOS, and any std C headers. 

The struct created within is for our custom BLE profile to save any connection data along with services and characteristics contained within for future use.

### callback_functions.c/h
Contains callback functions for gap and gatt event procedure status reporting.

### morse_functions.c/h
Contains all GPIO functions and interrupt service routines to control the read and write buttons. Each routine triggers a flag which is set and handled in the main polling task. There are also a few helper functions to encode our binary array input into the corresponding character array.

### poll_event_task_functions.c/h
Contains the task thread which would poll for the status of the flags which are to be set for the buttons. There is a read and write flag which when triggered would read and write from and to the server.

