cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Queijo LANGUAGES CXX C)

add_library(Queijo.Fs STATIC)
add_library(Queijo.Net STATIC)
add_library(Queijo.Task STATIC)

# luau setup
set(LUAU_BUILD_CLI OFF)
set(LUAU_BUILD_TESTS OFF)

add_subdirectory(extern/luau)

# libuv setup
add_subdirectory(extern/libuv)

# wolfssl setup
set(BUILD_SHARED_LIBS OFF)
set(WOLFSSL_CURL "yes")
set(WOLFSSL_EXAMPLES "no")
add_subdirectory(extern/wolfssl)

# curl setup
set(USE_LIBIDN2 OFF)
set(USE_NGHTTP2 OFF)
set(CURL_USE_LIBPSL OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_ZLIB OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_CURL_EXE OFF)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

set(CURL_USE_WOLFSSL ON)
set(WOLFSSL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/extern/wolfssl/include)
set(WOLFSSL_LIBRARY wolfssl)

add_subdirectory(extern/curl)

#

add_executable(Queijo.CLI)
set_target_properties(Queijo.CLI PROPERTIES OUTPUT_NAME queijo)

set(LIBUV_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/extern/libuv/include)

include(Sources.cmake)

target_compile_features(Queijo.Fs PUBLIC cxx_std_17)
target_compile_features(Queijo.Net PUBLIC cxx_std_17)
target_compile_features(Queijo.Task PUBLIC cxx_std_17)
target_include_directories(Queijo.Fs PUBLIC Fs/include ${LIBUV_INCLUDE_DIR})
target_include_directories(Queijo.Net PUBLIC Net/include ${LIBUV_INCLUDE_DIR})
target_include_directories(Queijo.Task PUBLIC Task/include ${LIBUV_INCLUDE_DIR})

target_link_libraries(Queijo.Fs PRIVATE Luau.VM uv)
target_link_libraries(Queijo.Net PRIVATE Luau.VM uv ${WOLFSSL_LIBRARY} libcurl)
target_link_libraries(Queijo.Task PRIVATE Luau.VM uv)
target_link_libraries(Queijo.CLI PRIVATE Luau.Compiler Luau.Config Luau.CodeGen Luau.VM Queijo.Fs Queijo.Net Queijo.Task)

target_compile_options(Queijo.Fs PRIVATE ${QUEIJO_OPTIONS})
target_compile_options(Queijo.Net PRIVATE ${QUEIJO_OPTIONS})
target_compile_options(Queijo.Task PRIVATE ${QUEIJO_OPTIONS})
target_compile_options(Queijo.CLI PRIVATE ${QUEIJO_OPTIONS})

set(QUEIJO_OPTIONS)

if(MSVC)
    list(APPEND QUEIJO_OPTIONS /D_CRT_SECURE_NO_WARNINGS) # We need to use the portable CRT functions.
    list(APPEND QUEIJO_OPTIONS "/we4018") # Signed/unsigned mismatch
    list(APPEND QUEIJO_OPTIONS "/we4388") # Also signed/unsigned mismatch
    list(APPEND QUEIJO_OPTIONS /WX) # Warnings are errors
else()
    list(APPEND QUEIJO_OPTIONS -Wall) # All warnings
    list(APPEND QUEIJO_OPTIONS -Wimplicit-fallthrough)
    list(APPEND QUEIJO_OPTIONS -Wsign-compare) # This looks to be included in -Wall for GCC but not clang
    list(APPEND QUEIJO_OPTIONS -Werror) # Warnings are errors
endif()
