# Fetch GTest external dependency
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 73a63ea05dc8ca29ec1d2c1d66481dd0de1950f1)
FetchContent_MakeAvailable(googletest)

# Build and setup tests
include(GoogleTest)
foreach(lib IN ITEMS tokenizer parser evaluator calculator)
    add_executable(${lib}Test ${lib}_test.cc)
    target_link_libraries(${lib}Test PRIVATE ${lib} GTest::gtest_main)
    gtest_discover_tests(${lib}Test) # Side effect: auto generates .gcda (coverage)
endforeach()

# Produce coverage report
find_program(GCOVR_PATH gcovr)
if(GCOVR_PATH)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning old coverage data..."
        COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete

        COMMAND ${CMAKE_COMMAND} -E echo "Running tests..."
        COMMAND ctest --output-on-failure

        COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage..."
        COMMAND ${GCOVR_PATH} -r ${CMAKE_SOURCE_DIR} --html --html-details
            --html-single-page --decisions --calls --html-theme github.dark-green 
            -o ${CMAKE_SOURCE_DIR}/coverage.html

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        BYPRODUCTS ${CMAKE_SOURCE_DIR}/coverage
        DEPENDS tokenizerTest parserTest evaluatorTest calculatorTest)
else()
    add_custom_target(coverage COMMAND false COMMENT "Gcovr not found")
endif()
