cmake_minimum_required(VERSION 3.13)

#include(pico_sdk_import.cmake)
include(../pico-sdk/pico_sdk_init.cmake)

project(pico-mdebug)

set(PICO_BOARD pico-mdebug)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

pico_sdk_init()

add_executable(
	pico-mdebug
	main.c
	swd-pio.c
	rswdp.c
)

pico_generate_pio_header(pico-mdebug ${CMAKE_CURRENT_LIST_DIR}/swd-io.pio)

target_include_directories(pico-mdebug PRIVATE .)

target_link_libraries(pico-mdebug PRIVATE hardware_pio pico_stdlib tinyusb_device tinyusb_board)

pico_add_extra_outputs(pico-mdebug)
