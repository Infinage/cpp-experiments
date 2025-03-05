cmake_minimum_required(VERSION 3.1...3.14)
project(webdriverxx LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing at the root level
enable_testing()

# Include directories
include_directories(include)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(cpr 
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG  1.11.2)
set(CPR_ENABLE_SSL OFF)
FetchContent_MakeAvailable(cpr)

# Define `webdriverxx` as an INTERFACE library
add_library(webdriverxx INTERFACE)

# Link libraries
target_link_libraries(webdriverxx INTERFACE cpr::cpr)

# Install headers and binary inside the build folder
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
    FILES_MATCHING PATTERN "*.hpp")

install(TARGETS webdriverxx
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# Include tests
add_subdirectory(test)
