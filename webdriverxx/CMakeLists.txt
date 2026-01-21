cmake_minimum_required(VERSION 3.28)
project(webdriverxx VERSION 0.0.1 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options for consumers
option(WEBDRIVERXX_BUILD_TESTS "Build webdriverxx tests" OFF)
option(WEBDRIVERXX_BUILD_EXAMPLES "Build webdriverxx examples" OFF)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.1/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=f3a6dcc6a04ce9e7f51a127307fa4f699fb2bade357a8eb4c5b45df76e1dc6a5
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
CPMAddPackage("gh:yhirose/cpp-httplib@0.30.1")
CPMAddPackage("gh:nlohmann/json@3.12.0")

# Define `webdriverxx` as an INTERFACE library
add_library(webdriverxx INTERFACE)
add_library(webdriverxx::webdriverxx ALIAS webdriverxx)
target_include_directories(webdriverxx INTERFACE include)
target_link_libraries(webdriverxx INTERFACE httplib::httplib nlohmann_json::nlohmann_json)
set_target_properties(webdriverxx PROPERTIES VERSION ${PROJECT_VERSION})

# Include tests
if (WEBDRIVERXX_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

# Include examples
if (WEBDRIVERXX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
