cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(HomeControlEvo C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories(${CMAKE_SOURCE_DIR}/lib)

# Add executable. Default name is the project name, version 1.1
add_executable(${PROJECT_NAME}
                HomeControlEvo.c
                lib/ssd1306.c
                lib/oledgfx.c
                lib/ws2812b.c
                lib/rgb.c
                lib/push_button.c
            )

pico_set_program_name(${PROJECT_NAME} "HomeControlEvo")
pico_set_program_version(${PROJECT_NAME} "1.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard library to the build
target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        hardware_gpio
        hardware_adc
        hardware_pio
        hardware_clocks
        hardware_i2c
        hardware_pwm
        pico_cyw43_arch_lwip_threadsafe_background
)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/lwip/src/include
    ${PICO_SDK_PATH}/lib/lwip/src/include/arch
    ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
)

target_sources(${PROJECT_NAME} PRIVATE
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/httpd.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/fs.c
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812b.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# Add any user requested libraries
pico_add_extra_outputs(${PROJECT_NAME})
