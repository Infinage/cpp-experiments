cmake_minimum_required(VERSION 3.30)
project(pacman LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.1/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=f3a6dcc6a04ce9e7f51a127307fa4f699fb2bade357a8eb4c5b45df76e1dc6a5
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
CPMAddPackage("gh:SFML/SFML#3.0.2")

file(GLOB PACMAN_SRC CONFIGURE_DEPENDS "src/*.cpp")

add_executable(pacman)
target_sources(pacman PRIVATE main.cpp ${PACMAN_SRC})
target_include_directories(pacman PRIVATE include)
target_link_libraries(pacman PRIVATE SFML::Graphics SFML::Window SFML::System)
