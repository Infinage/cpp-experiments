cmake_minimum_required(VERSION 3.30)
project(json-parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(json INTERFACE json.hpp)
target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(validate-json validate-json.cpp)
target_link_libraries(validate-json PRIVATE json)

add_executable(create-json create-json.cpp)
target_link_libraries(create-json PRIVATE json)

add_executable(manipulate-json manipulate-json.cpp)
target_link_libraries(manipulate-json PRIVATE json)

# Warnings for each of the executables
foreach(target validate-json create-json manipulate-json)
    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)
endforeach()

# Enable testing
enable_testing()
add_test(
    NAME json_test 
    COMMAND validate-json test/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
