file(GLOB_RECURSE LOVYAN_SOURCES "$ENV{RADIOHEAD_LOVYAN_DIR}/src/*.cpp"
    "$ENV{RADIOHEAD_LOVYAN_DIR}/src/*.c")
idf_component_register(SRCS ${LOVYAN_SOURCES}
    INCLUDE_DIRS "$ENV{RADIOHEAD_LOVYAN_DIR}/src"
    REQUIRES arduino esp_lcd efuse)
