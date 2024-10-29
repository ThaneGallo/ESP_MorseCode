# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/thane/esp/v5.2.2/esp-idf/components/bootloader/subproject"
  "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader"
  "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix"
  "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix/tmp"
  "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix/src"
  "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/thane/projects/ESP_MorseCode/Gatt_server/gatt_server/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
