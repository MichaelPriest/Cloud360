cmake_minimum_required(VERSION 3.25)
project(cloud360 C)

add_executable(cloud360
    src/main.c
    src/menu.c
    src/ui.c
    src/render.c
    src/icons.c
    src/settings.c
    src/network.c
    src/probe.c
    src/probe_platform.c
    src/https.c
    src/https_curl.c
    src/catalog.c
    src/catalog_runtime.c
)
set_target_properties(cloud360 PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(cloud360 PRIVATE -Wall -Wextra -Werror)

# HTTPS transport is optional during source development. Use only the
# XSDK-specific ports, never a host libcurl or insecure TLS fallback.
option(CLOUD360_ENABLE_SECURE_HTTP
       "Link curl-xenon + mbedtls-xenon with verified TLS 1.2" OFF)
set(CLOUD360_MBEDTLS_XENON_DIR "" CACHE PATH "mbedtls-xenon checkout")
set(CLOUD360_CURL_XENON_DIR "" CACHE PATH "curl-xenon checkout")
if(CLOUD360_ENABLE_SECURE_HTTP)
    if(NOT EXISTS "${CLOUD360_MBEDTLS_XENON_DIR}/CMakeLists.txt" OR
       NOT EXISTS "${CLOUD360_CURL_XENON_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "Provide CLOUD360_MBEDTLS_XENON_DIR and CLOUD360_CURL_XENON_DIR"
            " pointing to the XSDK xenon branches.")
    endif()
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CLOUD360_MBEDTLS_XENON_DIR}" mbedtls-xenon)
    # Follow curl-xenon's tests/xsdk/CMakeLists.txt dependency setup.
    set(MBEDTLS_INCLUDE_DIR "${CLOUD360_MBEDTLS_XENON_DIR}/include" CACHE PATH "" FORCE)
    set(MBEDTLS_LIBRARY mbedtls CACHE STRING "" FORCE)
    set(MBEDX509_LIBRARY mbedx509 CACHE STRING "" FORCE)
    set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory("${CLOUD360_CURL_XENON_DIR}" curl-xenon)
    target_link_libraries(cloud360 PRIVATE CURL::libcurl mbedtls)
    target_compile_definitions(cloud360 PRIVATE CLOUD360_WITH_CURL=1)
endif()

# XSDK requires major.minor.build.fix (four numeric components).
xsdk_add_xex(cloud360 TITLE_ID 0x434C4436 NAME "Cloud360" VERSION 0.2.0.0)
