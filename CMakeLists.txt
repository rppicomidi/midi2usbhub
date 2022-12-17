cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include(pico_sdk_import.cmake)

set(target_proj midi2usbhub)
project(${target_proj} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPICO_USE_MALLOC_MUTEX=1")
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/ext_lib/littlefs-lib ext_lib/littlefs-lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/ext_lib/fatfs/source)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/midi_uart_lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/rp2040_rtc)
set(EMBEDDED_CLI_PATH ${CMAKE_CURRENT_LIST_DIR}/ext_lib/embedded-cli/lib/)
add_executable(${target_proj}
    midi2usbhub.cpp
    preset_manager.cpp
    pico_lfs_cli.cpp
    pico_fatfs_cli.cpp
    preset_manager_cli.cpp
    midi2usbhub_cli.cpp
    ${EMBEDDED_CLI_PATH}/src/embedded_cli.c
    ${CMAKE_CURRENT_LIST_DIR}/ext_lib/parson/parson.c
)

pico_enable_stdio_uart(${target_proj} 1)

target_include_directories(${target_proj} PRIVATE
 ${CMAKE_CURRENT_LIST_DIR}
 ${EMBEDDED_CLI_PATH}/include
 ${CMAKE_CURRENT_LIST_DIR}/ext_lib/parson
)

target_link_options(${target_proj} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_proj} PRIVATE -Wall -Wextra -DPICO_HEAP_SIZE=0x20000 -DCFG_TUSB_DEBUG=1)
target_link_libraries(${target_proj} midi_uart_lib tinyusb_host tinyusb_board ring_buffer_lib pico_stdlib
littlefs-lib msc_fatfs rp2040_rtc)
if(DEFINED PICO_BOARD)
if(${PICO_BOARD} MATCHES "pico_w")
message("board is pico_w")
# add additional compile and link options
target_compile_options(${target_proj} PRIVATE -DRPPICOMIDI_PICO_W)
target_link_libraries(${target_proj} pico_cyw43_arch_none)
set(RPPICOMIDI_PICO_W 1)
else()
message("board is pico")
endif()
else()
message("board is not defined")
endif()

pico_add_extra_outputs(${target_proj})

