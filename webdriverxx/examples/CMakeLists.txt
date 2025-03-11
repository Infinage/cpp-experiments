# Find all .cpp files in the examples folder
file(GLOB EXAMPLE_SOURCES *.cpp)

foreach(EXAMPLE_FILE ${EXAMPLE_SOURCES})
    # Extract filename without extension
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)

    # Create executable
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})

    # Link against webdriverxx
    target_link_libraries(${EXAMPLE_NAME} PRIVATE webdriverxx)

    # Set output directory inside build/examples
    set_target_properties(${EXAMPLE_NAME} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endforeach()
