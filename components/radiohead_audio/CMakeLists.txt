file(GLOB_RECURSE AUDIO_SOURCES "${CMAKE_SOURCE_DIR}/lib/ESP32-audioI2S-master/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/lib/ESP32-audioI2S-master/src/*.c")
idf_component_register(SRCS ${AUDIO_SOURCES}
    INCLUDE_DIRS "../../lib/ESP32-audioI2S-master/src"
    REQUIRES arduino espressif__esp-dsp wear_levelling fatfs)
