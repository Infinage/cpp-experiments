function(Format)
    find_program(CLANG-FORMAT_PATH clang-format REQUIRED)

    set(options)
    set(oneValueArgs)
    set(multiValueArgs DIRECTORIES FILES)
    cmake_parse_arguments(FMT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(FMT_UNPARSED_ARGUMENTS OR (NOT FMT_DIRECTORIES AND NOT FMT_FILES))
        message(FATAL_ERROR "Usage: Format(DIRECTORIES <dirs...> [FILES <files...>])")
    endif()

    set(ALL_FILES)
    if(FMT_DIRECTORIES)
        foreach(dir IN LISTS FMT_DIRECTORIES)
            file(GLOB_RECURSE DIR_FILES LIST_DIRECTORIES false CONFIGURE_DEPENDS 
                "${dir}/*.h"
                "${dir}/*.hpp"
                "${dir}/*.hh"
                "${dir}/*.c"
                "${dir}/*.cc"
                "${dir}/*.cxx"
                "${dir}/*.cpp")
            list(APPEND ALL_FILES ${DIR_FILES})
        endforeach()
    endif()

    list(APPEND ALL_FILES ${FMT_FILES})
    list(REMOVE_DUPLICATES ALL_FILES)

    list(LENGTH ALL_FILES COUNT)

    add_custom_target(format
        COMMAND ${CLANG-FORMAT_PATH} -i --style=file ${ALL_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting ${COUNT} files...")
endfunction()
