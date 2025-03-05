message(STATUS "Including test directory")

# Define test executable
add_executable(webdriverxx_test test_startSession.cpp)

# Link against webdriverxx (which includes CPR)
target_link_libraries(webdriverxx_test PRIVATE webdriverxx)

# Ensure test sources can find headers
target_include_directories(webdriverxx_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Register the test with CTest
add_test(NAME WebDriverXXTest COMMAND webdriverxx_test)
