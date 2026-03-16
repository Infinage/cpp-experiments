cmake_minimum_required(VERSION 3.30)
project(url-shortner CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if none specified
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified. Defaulting to Release.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

foreach(obj IN ITEMS Server RequestHandler HashUtils)
    add_library(${obj} OBJECT)
    target_sources(${obj} 
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/${obj}.cpp
        PUBLIC FILE_SET HEADERS BASE_DIRS include
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/${obj}.hpp)
endforeach()

add_executable(url-shortner main.cpp)
target_link_libraries(url-shortner PRIVATE Server RequestHandler HashUtils)
target_link_options(url-shortner PRIVATE 
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:-static>)
