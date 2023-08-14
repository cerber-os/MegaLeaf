idf_component_register(
	SRCS    "main.cpp"
			"capabilities/caps_base.cpp"
			"capabilities/caps_switch.cpp"
			"capabilities/caps_switchLevel.cpp"
			"capabilities/caps_color.cpp"
			"capabilities/caps_colorEffect.cpp"
			"capabilities/caps_colorEffectSpeed.cpp"
			"comm/mlf_comm.c"
			"comm/mlf_protocol.c"
	EMBED_FILES	"cfg/device_info.json"
				"cfg/onboarding_config.json"
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/capabilities)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/comm)


set(stdk_config ${CMAKE_CURRENT_LIST_DIR}/stdkconfig.cmake)
add_subdirectory(../../st-device-sdk-c iotcore)
target_link_libraries(${COMPONENT_LIB} PUBLIC iotcore)
