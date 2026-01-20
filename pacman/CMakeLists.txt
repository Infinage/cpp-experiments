cmake_minimum_required(VERSION 3.30)
project(pacman LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.0/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
CPMAddPackage("gh:SFML/SFML#3.0.2")

file(GLOB PACMAN_SRC CONFIGURE_DEPENDS "src/*.cpp")

add_executable(pacman)
target_sources(pacman PRIVATE main.cpp ${PACMAN_SRC})
target_include_directories(pacman PRIVATE include)
target_link_libraries(pacman PRIVATE SFML::Graphics SFML::Window SFML::System)
