cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD vgaboard)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)
project(colour_text_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()
add_executable(colour_text
  main.c
  monprom.c
)
target_compile_definitions(colour_text PUBLIC -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=322
  -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8)

pico_enable_stdio_uart(colour_text 0)
pico_enable_stdio_usb(colour_text 0)
pico_add_extra_outputs(colour_text)
target_link_libraries(colour_text PRIVATE
  pico_stdlib
  pico_scanvideo_dpi
  pico_multicore)
