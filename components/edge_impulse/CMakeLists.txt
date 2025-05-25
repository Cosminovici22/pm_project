include(${COMPONENT_PATH}/edge-impulse-sdk/cmake/utils.cmake)

RECURSIVE_FIND_FILE_APPEND(SRCS . *.c)
RECURSIVE_FIND_FILE_APPEND(SRCS . *.cc)
RECURSIVE_FIND_FILE_APPEND(SRCS . *.cpp)

idf_component_register(
	SRCS ${SRCS}
	INCLUDE_DIRS .
	REQUIRES esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
