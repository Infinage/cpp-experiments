# $ ./geckodriver.exe --port=1001 & ./chromedriver.exe --port=1002 & ./msedgedriver.exe --port=1003 &
# $ ps | grep "driver" | awk '{print $1}' | xargs kill -9
# $ FIREFOX_PORT=1001 CHROME_PORT=1002 MSEDGE_PORT=1003 ctest --output-on-failure

# Find all test source files
file(GLOB TEST_SOURCES test_*.cpp)

# Define test configurations
set(BROWSERS "FIREFOX" "CHROME" "MSEDGE")
set(BINARY_PATHS 
    "C:/Program Files/Mozilla Firefox/firefox.exe"
    "C:/Program Files/Google/Chrome/Application/chrome.exe"
    "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
)

list(LENGTH BROWSERS NUM_BROWSERS)
math(EXPR NUM_BROWSERS "${NUM_BROWSERS} - 1")

foreach(TEST_SOURCE ${TEST_SOURCES})
    # Extract file name without extension
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # Define test executable
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    # Link against webdriverxx
    target_link_libraries(${TEST_NAME} PRIVATE webdriverxx)

    # Ensure test sources can find headers
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

    # Register the test multiple times with different env variables
    foreach(i RANGE ${NUM_BROWSERS})
        list(GET BROWSERS ${i} BROWSER)
        list(GET BINARY_PATHS ${i} BINARY)

        add_test(NAME ${TEST_NAME}_${BROWSER} COMMAND ${TEST_NAME})

        # Lower case labels for tests
        string(TOLOWER "${BROWSER}" BROWSER_LOWER)

        # Set environment variables for this test case
        set_tests_properties(${TEST_NAME}_${BROWSER} PROPERTIES
            ENVIRONMENT "BROWSER=${BROWSER};BROWSER_BINARY=${BINARY}"
            LABELS "${BROWSER_LOWER}"  # Assign a label based on the browser
        )
    endforeach()
endforeach()
