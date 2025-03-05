# Find all test source files
file(GLOB TEST_SOURCES test_*.cpp)

foreach(TEST_SOURCE ${TEST_SOURCES})
    # Extract file name without extension
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # Define test executable
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    # Link against webdriverxx
    target_link_libraries(${TEST_NAME} PRIVATE webdriverxx)

    # Ensure test sources can find headers
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

    # Register the test with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
