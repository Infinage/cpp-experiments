cmake_minimum_required(VERSION 3.30)
project(url-shortner CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type if none specified
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified. Defaulting to Release.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

include_directories(include)
file(GLOB SOURCES src/*.cpp)
add_executable(url-shortner main.cpp ${SOURCES})

# Debug build
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0")

# Release build
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
target_link_options(url-shortner PRIVATE "$<$<CONFIG:Release>:-static>")

# Output binary renaming per-config
set_target_properties(url-shortner PROPERTIES
    OUTPUT_NAME_RELEASE "app"
    OUTPUT_NAME_DEBUG "app.debug"
)
