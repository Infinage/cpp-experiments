cmake_minimum_required(VERSION 4.0)
project(Calc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEV_MODE "Developer Mode" OFF)
message("Build type is ${CMAKE_BUILD_TYPE}")

if (DEV_MODE)
    message("Developer mode is enabled")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # Enable project wide clang-tidy check
    # Can set target level properties: CXX_CLANG_TIDY
    find_program(CLANG-TIDY_PATH clang-tidy)
    if (CLANG-TIDY_PATH)
        message("Enabling clang-tidy static analysis check")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG-TIDY_PATH};-checks=*")
    endif(CLANG-TIDY_PATH)

    include(${CMAKE_SOURCE_DIR}/cmake/Format.cmake)
    Format(DIRECTORIES src test)

    if (CMAKE_BUILD_TYPE STREQUAL Debug)
        include(CheckCXXCompilerFlag)
        set(flag undefined)
        check_cxx_compiler_flag(-fsanitize=${flag} HAS_SANITIZER_FLAG)
        if (HAS_SANITIZER_FLAG)
            message("Adding undefined sanitizer flag")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${flag} -fno-omit-frame-pointer")
            set(DCMAKE_C_FLAGS "${DCMAKE_C_FLAGS} -fsanitize=${flag} -fno-omit-frame-pointer")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${flag}")
            set(DCMAKE_MODULE_LINKER_FLAGS "${DCMAKE_MODULE_LINKER_FLAGS} -fsanitize=${flag}")
        endif()
    endif()

    # Add doxygen target to auto generate docs
    find_package(Doxygen)
    if (NOT DOXYGEN_FOUND)
        add_custom_target(doxygen COMMAND false COMMENT "Doxygen not found")
    else()
        set(DOXYGEN_PROJECT_NAME Calculator)
        set(DOXYGEN_PROJECT_BRIEF "Calculator App")
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_HTML_OUTPUT ${CMAKE_SOURCE_DIR}/docs) 
        set(DOXYGEN_SOURCE_BROWSER YES)
        set(DOXYGEN_INLINE_SOURCES YES)
        set(DOXYGEN_HAVE_DOTS YES)
        set(DOXYGEN_CLASS_GRAPH YES)
        set(DOXYGEN_CALL_GRAPH YES)
        set(DOXYGEN_CALLER_GRAPH YES)
        doxygen_add_docs(doxygen ${CMAKE_SOURCE_DIR}/src COMMENT "Generating HTML documentation")
    endif()

    include(CTest)
    add_subdirectory(test)

endif(DEV_MODE)

add_subdirectory(src)
