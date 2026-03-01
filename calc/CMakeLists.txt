cmake_minimum_required(VERSION 4.0)
project(Calc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEV_MODE "Developer Mode" OFF)
message("Developer mode is ${DEV_MODE}")

if (DEV_MODE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    include(CTest)
    add_subdirectory(test)
endif(DEV_MODE)

add_subdirectory(src)
