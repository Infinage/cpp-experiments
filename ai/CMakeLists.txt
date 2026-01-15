cmake_minimum_required(VERSION 3.18)
project(ai-experiments LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23 REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.0/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage("gh:TheLartians/Ccache.cmake@1.2.5")

CPMAddPackage(
    URI "gh:ggml-org/llama.cpp#b7726" 
    OPTIONS 
        "LLAMA_STANDALONE OFF" 
        "LLAMA_BUILD_COMMON ON"
)

add_executable(embedding-sample)
target_sources(embedding-sample PRIVATE embedding.cpp)
target_link_libraries(embedding-sample PRIVATE common llama)
