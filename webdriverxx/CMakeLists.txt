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

# Set CPPLIB root directory
set(CPPLIB_DIR "$ENV{HOME}/cpplib")
set(CPPLIB_REPOS_DIR "${CPPLIB_DIR}/repos")

# Check if CPR is available in ~/cpplib
find_library(CPR_LIB NAMES cpr PATHS "${CPPLIB_DIR}/lib" NO_DEFAULT_PATH)
find_path(CPR_INCLUDE_DIR NAMES cpr/cpr.h PATHS "${CPPLIB_DIR}/include" NO_DEFAULT_PATH)

if (CPR_LIB AND CPR_INCLUDE_DIR)
    message(STATUS "Using prebuilt CPR from ${CPPLIB_DIR}")
    
    add_library(cpr::cpr INTERFACE IMPORTED)
    set_target_properties(cpr::cpr PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CPR_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${CPR_LIB}"
    )
else()
    message(STATUS "CPR not found in ${CPPLIB_DIR}, fetching from GitHub...")

    include(FetchContent)
    FetchContent_Declare(cpr 
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.11.2
        SOURCE_DIR "${CPPLIB_REPOS_DIR}/cpr"
    )

    set(CPR_ENABLE_SSL OFF)

    # Install fetched CPR to ~/cpplib
    set(CMAKE_INSTALL_PREFIX "${CPPLIB_DIR}")
    FetchContent_MakeAvailable(cpr)
endif()

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
