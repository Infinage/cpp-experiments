cmake_minimum_required(VERSION 3.27)
project(InfinageProjects LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ------------------------------
# CPM + Ccache setup
# ------------------------------
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.1/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=f3a6dcc6a04ce9e7f51a127307fa4f699fb2bade357a8eb4c5b45df76e1dc6a5
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage("gh:TheLartians/Ccache.cmake@1.2.5")
set(USE_CCACHE ON CACHE BOOL "Enable ccache" FORCE)

message(STATUS "Ccache enabled: ${USE_CCACHE}")

# ------------------------------
# Targets list
# ------------------------------

if(NOT DEFINED BUILD_LIST)
    set(BUILD_LIST "cjudge,brainfuck,csvsplit,grid2png,linear-regression,mandelbrot,matrix,minesweeper,ts-playground")
endif()
string(REPLACE "," ";" BUILD_LIST ${BUILD_LIST})
message(STATUS "Targets to build: ${BUILD_LIST}")

# ------------------------------
# Individual targets
# ------------------------------

# minesweeper (requires FTXUI)
if("minesweeper" IN_LIST BUILD_LIST)
    CPMAddPackage("gh:ArthurSonzogni/FTXUI@6.1.9")
    add_executable(minesweeper minesweeper.cpp)
    target_link_libraries(minesweeper PRIVATE component)
endif()

# mandelbrot (requires SFML)
if("mandelbrot" IN_LIST BUILD_LIST)
    find_package(OpenMP REQUIRED)
    CPMAddPackage("gh:SFML/SFML#3.0.2")
    add_executable(mandelbrot mandelbrot.cpp)
    target_compile_options(mandelbrot PRIVATE -march=native -ffast-math)
    target_link_libraries(mandelbrot PRIVATE 
        SFML::Graphics SFML::Window SFML::System 
        OpenMP::OpenMP_CXX
    )
endif()

# grid2png (requires SFML)
if("grid2png" IN_LIST BUILD_LIST)
    CPMAddPackage("gh:SFML/SFML#3.0.2")
    add_executable(grid2png grid2png.cpp)
    target_link_libraries(grid2png PRIVATE SFML::Graphics SFML::Window)
endif()

# linear-regression (requires pybind11)
if("linear-regression" IN_LIST BUILD_LIST)
    CPMAddPackage("gh:pybind/pybind11@3.0.1")
    pybind11_add_module(CPPLearn linear-regression.cpp)
endif()

# ts-playground (requires tree-sitter, tree-sitter-cpp)
if("ts-playground" IN_LIST BUILD_LIST)
    CPMAddPackage("gh:tree-sitter/tree-sitter@0.26.3")
    CPMAddPackage("gh:tree-sitter/tree-sitter-cpp@0.23.4")
    add_executable(ts-playground ts-playground.cpp)
    target_link_libraries(ts-playground tree-sitter tree-sitter-cpp)
endif()

# cjudge (cap linker flag required)
if("cjudge" IN_LIST BUILD_LIST)
    add_executable(cjudge cjudge.cpp)
    target_link_options(cjudge PRIVATE -lcap)
endif()

# brainfuck (no deps)
if("brainfuck" IN_LIST BUILD_LIST)
    add_executable(brainfuck brainfuck.cpp)
endif()

# csvsplit (no deps)
if("csvsplit" IN_LIST BUILD_LIST)
    add_executable(csvsplit csvsplit.cpp)
endif()

# matrix (no deps)
if("matrix" IN_LIST BUILD_LIST)
    add_executable(matrix matrix.cpp)
endif()
