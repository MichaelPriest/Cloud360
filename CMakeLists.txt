cmake_minimum_required(VERSION 3.25)
project(cloud360 C)

add_executable(cloud360 src/main.c)
set_target_properties(cloud360 PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(cloud360 PRIVATE -Wall -Wextra -Werror)

# XSDK requires major.minor.build.fix (four numeric components).
xsdk_add_xex(cloud360 TITLE_ID 0x434C4436 NAME "Cloud360" VERSION 0.1.0.0)
