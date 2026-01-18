cmake_minimum_required(VERSION 3.27)
project(cpp_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

# Discover all cpp files
file(GLOB CPP_SOURCES CONFIGURE_DEPENDS "*.cpp")

# Optional build list (comma-separated)
if(DEFINED BUILD_LIST)
    string(REPLACE "," ";" BUILD_TARGETS "${BUILD_LIST}")
    message(STATUS "Targets requested: ${BUILD_TARGETS}")
else()
    set(BUILD_TARGETS "")
    foreach(src ${CPP_SOURCES})
        get_filename_component(name ${src} NAME_WE)
        list(APPEND BUILD_TARGETS ${name})
    endforeach()
    message(STATUS "Building all targets: ${BUILD_TARGETS}")
endif()

# Build only selected targets
foreach(src ${CPP_SOURCES})
    get_filename_component(target ${src} NAME_WE)

    if(target IN_LIST BUILD_TARGETS)
        add_executable(${target} ${src})

        # Special-case linking
        if(target STREQUAL "ascii-art")
            find_package(PNG REQUIRED)
            target_link_libraries(${target} PRIVATE PNG::PNG)
        endif()
    endif()
endforeach()
