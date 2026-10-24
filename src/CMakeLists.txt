if("$ENV{RADIOHEAD_TFT_COLOR_TEST}" STREQUAL "ON")
    idf_component_register(SRCS "../experiments/tft-color-test/src/main.cpp"
        REQUIRES arduino radiohead_lovyangfx)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RADIOHEAD_TFT_COLOR_TEST=1)
else()
    # ESP-IDF also evaluates this file in script mode to discover requirements.
    if(CMAKE_SCRIPT_MODE_FILE)
        file(GLOB RADIOHEAD_SOURCES "*.cpp")
    else()
        file(GLOB RADIOHEAD_SOURCES CONFIGURE_DEPENDS "*.cpp")
    endif()
    idf_component_register(SRCS ${RADIOHEAD_SOURCES}
        INCLUDE_DIRS "../include" "../.pio/ui_assets"
        REQUIRES arduino radiohead_audio radiohead_lovyangfx radiohead_arduinojson
            radiohead_spotify espressif__expat)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RADIO_SPOTIFY_EXPERIMENT=1)
endif()
