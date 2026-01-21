cmake_minimum_required(VERSION 3.30)
project(Redis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TEST "Build tests" OFF)
message(STATUS "Build tests: ${BUILD_TEST}")

# ---- Core library ----
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(redis_core STATIC ${CORE_SOURCES})
target_include_directories(redis_core PUBLIC src)

# ---- Server executable ----
add_executable(server main.cpp)
target_link_libraries(server PRIVATE redis_core)

# ---- Tests ----
if (BUILD_TEST)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS test/*.cpp)
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE redis_core)
    endforeach()
endif()
