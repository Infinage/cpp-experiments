cmake_minimum_required(VERSION 3.30)
project(cryptography LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23 REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

file(GLOB CPP_SOURCES CONFIGURE_DEPENDS "*.cpp")

foreach(src ${CPP_SOURCES})
    get_filename_component(target ${src} NAME_WE)
    add_executable(${target} ${src})
endforeach()
