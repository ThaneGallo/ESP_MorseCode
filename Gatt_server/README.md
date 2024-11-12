# Gatt_Server

## Organization

morse_server.c is our main file and exists within main folder and our headers exist within the same directory 

By default the device name is "BLE-server".

### Morse_mbuf
Contains helper functions to make creating mempools easier for the user to allow for the server to save any written data to a secondary mbuf for temporary storage until the next write event occurs.
