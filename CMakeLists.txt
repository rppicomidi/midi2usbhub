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
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pio_midi_uart_lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pico-ssd1306-mono-graphics-lib lib/pico-ssd1306-mono-graphics-lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pico-mono-ui-lib lib/pico-mono-ui-lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/rp2040_rtc)
set(EMBEDDED_CLI_PATH ${CMAKE_CURRENT_LIST_DIR}/ext_lib/embedded-cli/lib/)
add_executable(${target_proj}
    midi2usbhub.cpp
    preset_manager.cpp
    pico_lfs_cli.cpp
    pico_fatfs_cli.cpp
    preset_manager_cli.cpp
    midi2usbhub_cli.cpp
    home_screen.cpp
    wifi_setup_menu.cpp
    wifi_scan_menu.cpp
    wifi_manual_setup_view.cpp
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
target_compile_options(${target_proj} PRIVATE -Wall -Wextra -DCFG_TUSB_DEBUG=1)
target_link_libraries(${target_proj} pio_midi_uart_lib tinyusb_host tinyusb_board ring_buffer_lib pico_stdlib
ssd1306 ssd1306i2c text_box mono_graphics_lib ui_menu ui_view_manager ui_hid_keyboard ui_text_item_chooser
ui_text_entry_box littlefs-lib msc_fatfs rp2040_rtc)
if(DEFINED PICO_BOARD)
if(${PICO_BOARD} MATCHES "pico_w")
message("board is pico_w")
set(FS_DATA_DIR ${CMAKE_CURRENT_LIST_DIR}/fsdata)
set(MAKE_FS_DATA_SCRIPT ${FS_DATA_DIR}/makefsdata)

add_custom_command(
    OUTPUT ${FS_DATA_DIR}/custom_fsdata.c
    COMMAND ${MAKE_FS_DATA_SCRIPT}
    DEPENDS 
        ${FS_DATA_DIR}/fs/index.html
        ${FS_DATA_DIR}/fs/404.html
        ${FS_DATA_DIR}/fs/429.html
        ${FS_DATA_DIR}/fs/css/style.css
        ${FS_DATA_DIR}/fs/css/siimple.min.css
        ${FS_DATA_DIR}/fs/img/favicon.ico
    WORKING_DIRECTORY ${FS_DATA_DIR}
)
add_custom_target(run ALL
    DEPENDS ${FS_DATA_DIR}/custom_fsdata.c)
set_property(SOURCE ${PICO_SDK_PATH}/lib/lwip/src/apps/http/fs.c APPEND PROPERTY OBJECT_DEPENDS ${FS_DATA_DIR}/custom_fsdata.c)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pico-w-connection-manager)
target_compile_options(${target_proj} PRIVATE -DRPPICOMIDI_PICO_W)
target_link_libraries(${target_proj} pico_w_connection_manager pico_lwip_http)
target_include_directories(${target_proj} PRIVATE
 ${CMAKE_CURRENT_LIST_DIR}/lib/pico-c-utilities/
)
target_sources(${target_proj} PRIVATE
 pico_w_connection_manager_cli.cpp
 ${CMAKE_CURRENT_LIST_DIR}/lib/pico-c-utilities/getsn.c
)
set(RPPICOMIDI_PICO_W 1)
else()
message("board is pico")
endif()
else()
message("board is not defined")
endif()

pico_add_extra_outputs(${target_proj})

