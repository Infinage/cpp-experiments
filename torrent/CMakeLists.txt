cmake_minimum_required(VERSION 4.0)
project(ctorrent CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Debug
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

file(GLOB SOURCES src/*.cpp)
add_executable(ctorrent main.cpp ${SOURCES})

find_package(OpenSSL REQUIRED)
target_link_libraries(ctorrent OpenSSL::SSL OpenSSL::Crypto)

# Build-type specific flags
target_compile_options(ctorrent PRIVATE
    $<$<CONFIG:Debug>:-O0 -ggdb3>
    $<$<CONFIG:Release>:-O3 -march=native -DNDEBUG>
)

target_compile_options(ctorrent PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion
)
