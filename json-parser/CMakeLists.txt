cmake_minimum_required(VERSION 3.30)
project(json-parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(json INTERFACE json.hpp)
target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

OPTION(BUILD_JSON_EXAMPLES "Build json executables" OFF)
if (BUILD_JSON_EXAMPLES)
    foreach(target validate-json create-json manipulate-json)
        add_executable(${target} ${target}.cpp)
        target_link_libraries(${target} PRIVATE json)
        target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)
    endforeach()

    # Enable testing
    enable_testing()
    add_test(
        NAME json_test COMMAND validate-json test/
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
