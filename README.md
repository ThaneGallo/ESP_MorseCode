# ESP_MorseCode
## Abstract
This project emulates morse code communication, where one ESP32 board, the client, receives a morse code input from the user and sends it as a BLE packet to the second ESP32 board, the server.

This is done using two ESP32 development boards and the [ESP-IDF](https://github.com/espressif/esp-idf) IoT development framework as well as the [Apache Mynewt NimBLE](https://mynewt.apache.org/latest/network/index.html) Bluetooth Low Energy (BLE) stack.


## Introduction


FreeRTOS, GPIO, RTC, ISR,

There are two buttons implimented on the GPIO pins of the development board with multiple interrupt service routines (ISRs) to minimize polling and resource utilization. 

Of these two buttons, one for the writing and encoding of the message and the other for sending it to the server device. 

The write/encode button functions as normal for morse code communication, where short presses and long presses (dots or dashes) are mapped to 0s and 1s in our code, respectively. Letters are seperated by 2s and are signified by a 2 second inteveral between button presses by the user. Before sending, the binary input array is converted into a character array according to the morse encoding resulting in a 28&% reduction of packet size.



## BLE Structure


## How to use


### Hardware Setup

![Hardware Setup](Images/ESP-32_MorseCode.png)


### What to flash and how

There are currently two differnet files to be flashed, one for the client and one for the server. They are in their directories labelled gatt_client and gatt_server respectively. Simply use the flashing tool in the ESP-IDF toolkit and flash each dev board and they should pair with one another and begin communications.



### Using the buttons

GPIO 4 and 5 are both connected to one of the buttons they trigger an interrupt on the negative edge and positive edge of the gpio signal respectively. This button measures the time between events using the RTC on the development board and compares to a global value which is currently set to one second (1000000 microseconds). If the time between events is above one second it fills the message buffer with a 1 and if it is below it is filled with a 0. After a character is written there must be no input for two seconds then the next character can be written.

Once the message is completed use the other button which is tied to GPIO 23 which encodes the message the buffer and saves it to a character buffer to speed up transmission by sending the ecoded bytes going from an average of 3.6 bytes per character to 1 byte which effectively less than one third of the original message size.






## Future Works
