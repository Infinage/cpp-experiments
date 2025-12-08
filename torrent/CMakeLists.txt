cmake_minimum_required(VERSION 3.28)
project(ctorrent CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Static build option ----
option(STATIC_BUILD "Link statically" OFF)
if(STATIC_BUILD)
    message(STATUS "Static build enabled")
    set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
else()
    message(STATUS "Static build disabled")
endif()

# ---- Default build type = Release ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Defaulting to Release build")
endif()

file(GLOB SOURCES src/*.cpp)
add_executable(ctorrent main.cpp ${SOURCES})

find_package(OpenSSL REQUIRED)
target_link_libraries(ctorrent OpenSSL::SSL OpenSSL::Crypto)

# ---- Compile flags ----
target_compile_options(ctorrent PRIVATE
    $<$<CONFIG:Debug>:-O0 -ggdb3>
    $<$<CONFIG:Release>:-O3 -march=native -DNDEBUG>
    -Wall -Wextra -Wpedantic -Wconversion
)
