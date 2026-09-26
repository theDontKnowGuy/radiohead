if(DEFINED ENV{RADIOHEAD_CSPOT_DIR})
    idf_component_register(SRCS "spotify_adapter.cpp" "MAX98357AAudioSink.cpp"
        INCLUDE_DIRS "../../include"
        REQUIRES arduino esp_wifi esp_event nvs_flash spiffs espressif__mdns
            freertos esp_netif esp_timer mbedtls driver esp_driver_gpio
            esp_driver_usb_serial_jtag esp_driver_i2c esp_driver_i2s)

    set(BELL_DISABLE_SINKS ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory("$ENV{RADIOHEAD_CSPOT_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/cspot_build" EXCLUDE_FROM_ALL)
    # cspot's Bell HTTPClient.h shares a basename with Arduino's header.
    target_include_directories(${COMPONENT_LIB} BEFORE PRIVATE
        "$ENV{RADIOHEAD_CSPOT_DIR}/bell/main/io/include")
    if(TARGET bell)
        target_include_directories(bell PUBLIC
            "$ENV{IDF_PATH}/components/mbedtls/mbedtls/include"
            "$ENV{IDF_PATH}/components/mbedtls/mbedtls/tf-psa-crypto/include"
            "$ENV{IDF_PATH}/components/mbedtls/port/include"
            "$ENV{IDF_PATH}/components/mbedtls/mbedtls/tf-psa-crypto/drivers/builtin/include"
            "${CMAKE_BINARY_DIR}/config")
        target_compile_definitions(bell PUBLIC MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS)
    endif()
    target_link_libraries(${COMPONENT_LIB} PUBLIC cspot)
    target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17 -Wno-missing-field-initializers)
    foreach(_external opencore-aacdec opus tremor civetweb mdnssvc bell_nanopb civetweb-cpp bell)
        if(TARGET ${_external})
            target_compile_options(${_external} PRIVATE -w)
        endif()
    endforeach()
    if(TARGET bell)
        target_compile_definitions(bell PRIVATE HAVE_TIMEGM)
    endif()
else()
    idf_component_register()
endif()
