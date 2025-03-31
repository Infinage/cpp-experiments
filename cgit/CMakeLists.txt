cmake_minimum_required(VERSION 3.10...3.14)
project(cgit CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
find_package(ZLIB REQUIRED)

file(GLOB SOURCES src/*.cpp)
add_executable(cgit main.cpp ${SOURCES})

target_link_libraries(cgit ZLIB::ZLIB)
