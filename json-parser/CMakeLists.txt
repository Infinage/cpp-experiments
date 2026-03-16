cmake_minimum_required(VERSION 3.30)
project(json-parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

OPTION(JSON_BUILD_EXAMPLES "Build Examples" OFF)
OPTION(JSON_BUILD_TESTS "Build Tests" OFF)
OPTION(JSON_BUILD_TESTS_EXTRA "Build Extra Tests" OFF)

add_library(json INTERFACE)
target_sources(json INTERFACE FILE_SET HEADERS FILES json/json.hpp)

set(JSON_TARGET_LIST)

if (JSON_BUILD_EXAMPLES)
    set(JSON_TARGET_LIST validate-json create-json manipulate-json)
endif()

if(JSON_BUILD_TESTS OR JSON_BUILD_TESTS_EXTRA)
    set(JSON_TARGET_LIST validate-json)
    list(REMOVE_DUPLICATES JSON_TARGET_LIST)
endif()

foreach(target IN LISTS JSON_TARGET_LIST)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE json)
    target_compile_options(${target} PRIVATE -Wall -Wextra)
endforeach()

if (JSON_BUILD_TESTS OR JSON_BUILD_TESTS_EXTRA)
    include(CTest)
    add_test(NAME json_test_basic COMMAND validate-json test/ 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    if(JSON_BUILD_TESTS_EXTRA)
        add_test(NAME json_test_extra COMMAND validate-json test/extras/ 
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()
