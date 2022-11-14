idf_component_register(SRCS "main.cpp"
                            "capabilities/caps_base.cpp"
                            "capabilities/caps_switch.cpp"
                            "capabilities/caps_switchLevel.cpp"
                            "capabilities/caps_color.cpp"
                            "capabilities/caps_colorEffect.cpp"
                            "capabilities/caps_colorEffectSpeed.cpp"
                            "comm/mlf_comm.c"
                            "comm/mlf_protocol.c"
                    EMBED_FILES "cfg/device_info.json"
                                "cfg/onboarding_config.json"
                    )

set(STDK_IOT_CORE_USE_DEFINED_CONFIG "y")

set(STDK_LINK_LIBRARY
        __idf_libsodium
        __idf_json
   )

set(STDK_INCLUDE_PATH
        "$ENV{IDF_PATH}/components/freertos/include/freertos"
        "$ENV{IDF_PATH}/components/nvs_flash/include"
        "$ENV{IDF_PATH}/components/spi_flash/include"
        "$ENV{IDF_PATH}/components/bootloader_support/include"
    )

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/capabilities)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/comm)
add_subdirectory($ENV{STDK_CORE_PATH} iotcore)
target_link_libraries(${COMPONENT_LIB} PUBLIC iotcore)
