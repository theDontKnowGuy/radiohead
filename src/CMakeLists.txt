file(GLOB RADIOHEAD_SOURCES "*.cpp")
if(DEFINED ENV{RADIOHEAD_IDF_INTEGRATED})
    idf_component_register(SRCS ${RADIOHEAD_SOURCES}
        INCLUDE_DIRS "../include" "../.pio/ui_assets"
        REQUIRES arduino radiohead_audio radiohead_lovyangfx radiohead_arduinojson
            radiohead_spotify)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RADIO_SPOTIFY_EXPERIMENT=1)
else()
    idf_component_register(SRCS ${RADIOHEAD_SOURCES} INCLUDE_DIRS "../include")
endif()
