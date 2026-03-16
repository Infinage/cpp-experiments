cmake_minimum_required(VERSION 3.30)
project(Redis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    message(STATUS "No build type specified, defaulting to Release")
endif()

option(BUILD_TESTS "Build tests" OFF)
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ---- Core library ----
add_library(redis_core)
target_sources(redis_core PRIVATE 
    PRIVATE 
        src/Cache.cpp 
        src/CommandHandler.cpp 
        src/Node.cpp 
        src/Server.cpp 
        src/Utils.cpp

    PUBLIC FILE_SET HEADERS BASE_DIRS include 
    FILES 
        include/Cache.hpp
        include/CommandHandler.hpp
        include/Node.hpp
        include/Server.hpp
        include/Utils.hpp
)

# ---- Server executable ----
add_executable(redis-server main.cpp)
target_link_libraries(redis-server PRIVATE redis_core)

# ---- Tests ----
if (BUILD_TESTS)
    include(CTest)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS test/*.cpp)
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(file_name ${test_src} NAME_WE)

        # expiry-test to expiry
        string(REGEX MATCH "^[a-z]+" base ${file_name})

        # Capitialize string and prepend test: testExpiry
        string(SUBSTRING ${base} 0 1 first)
        string(TOUPPER ${first} first)
        string(SUBSTRING ${base} 1 -1 rest)
        set(test_name "test${first}${rest}")

        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE redis_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
