cmake_minimum_required(VERSION 3.25)
project(cloud360 C)

set(CLOUD360_COMMON
    src/main.c
    src/menu.c
    src/ui.c
    src/render.c
    src/icons.c
    src/settings.c
)

# Xbox 360 console: native XSDK display controller + 720p presentation.
add_executable(cloud360 ${CLOUD360_COMMON})
set_target_properties(cloud360 PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(cloud360 PRIVATE -Wall -Wextra -Werror)
xsdk_add_xex(cloud360 TITLE_ID 0x434C4436 NAME "Cloud360" VERSION 0.2.1.0)

# Xenia Canary: VdSwap frontbuffer fallback (experimental emulator support).
add_executable(cloud360_xenia ${CLOUD360_COMMON} src/xenia_present.c)
set_target_properties(cloud360_xenia PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(cloud360_xenia PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(cloud360_xenia PRIVATE CLOUD360_XENIA=1)
xsdk_add_xex(cloud360_xenia TITLE_ID 0x434C5836 NAME "Cloud360 Xenia" VERSION 0.2.1.0)
