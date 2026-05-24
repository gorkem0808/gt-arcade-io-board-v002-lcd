cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(gt_arcade_kontrol_io_board_v002_lcd C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

function(add_gt_arcade_target TARGET PLAYER)
    add_executable(${TARGET}
        src/main.c
        src/usb_descriptors.c
        src/lcd_i2c.c
        src/config_store.c
    )

    target_compile_definitions(${TARGET} PRIVATE
        PLAYER_ID=${PLAYER}
    )

    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
    )

    target_link_libraries(${TARGET}
        pico_stdlib
        hardware_adc
        hardware_i2c
        hardware_flash
        hardware_sync
        tinyusb_device
        tinyusb_board
    )

    pico_enable_stdio_usb(${TARGET} 0)
    pico_enable_stdio_uart(${TARGET} 0)
    pico_add_extra_outputs(${TARGET})
endfunction()

add_gt_arcade_target(gt_arcade_io_p1_lcd_v002 1)
add_gt_arcade_target(gt_arcade_io_p2_lcd_v002 2)
