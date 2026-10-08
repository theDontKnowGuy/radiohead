# ESP-IDF also evaluates this file in script mode to discover requirements.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB RADIOHEAD_SOURCES "*.cpp")
else()
    file(GLOB RADIOHEAD_SOURCES CONFIGURE_DEPENDS "*.cpp")
endif()
if(DEFINED ENV{RADIOHEAD_IDF_INTEGRATED})
    idf_component_register(SRCS ${RADIOHEAD_SOURCES}
        INCLUDE_DIRS "../include" "../.pio/ui_assets"
        REQUIRES arduino radiohead_audio radiohead_lovyangfx radiohead_arduinojson
            radiohead_spotify)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RADIO_SPOTIFY_EXPERIMENT=1)
else()
    idf_component_register(SRCS ${RADIOHEAD_SOURCES} INCLUDE_DIRS "../include")
endif()
