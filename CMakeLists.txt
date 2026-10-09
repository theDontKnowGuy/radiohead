cmake_minimum_required(VERSION 3.16.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
if(NOT DEFINED ENV{RADIOHEAD_ARDUINO_DIR})
    message(FATAL_ERROR "Run python3 tools/build_firmware.py to prepare the pinned components")
endif()
set(EXTRA_COMPONENT_DIRS "$ENV{RADIOHEAD_ARDUINO_DIR}" "${CMAKE_SOURCE_DIR}/src")
add_compile_definitions(CORE_DEBUG_LEVEL=3 BOARD_HAS_PSRAM
    ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1
    ARDUINO_LOOP_STACK_SIZE=16384 FIRMWARE_BUILD="esp32s3")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(radiohead)
