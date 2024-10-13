# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/alberti/esp/v5.0.6/esp-idf/components/bootloader/subproject"
  "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader"
  "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix"
  "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix/tmp"
  "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix/src/bootloader-stamp"
  "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix/src"
  "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/alberti/Documents/Code/ESP/Projete2k24/LORA_WIFI_GATEWAY--master_Station/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
