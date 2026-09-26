cmake_minimum_required(VERSION 3.16.0)
if(DEFINED ENV{RADIOHEAD_IDF_INTEGRATED})
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/.pio/spotify-components/arduino"
        "${CMAKE_SOURCE_DIR}/src")
    add_compile_definitions(CORE_DEBUG_LEVEL=3 BOARD_HAS_PSRAM
        ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1
        ARDUINO_LOOP_STACK_SIZE=16384 FIRMWARE_BUILD="esp32s3")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(radiohead)
