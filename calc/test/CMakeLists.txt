include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 73a63ea05dc8ca29ec1d2c1d66481dd0de1950f1)
FetchContent_MakeAvailable(googletest)

add_executable(tokenizerTest tokenizer_test.cc)
target_link_libraries(tokenizerTest PRIVATE tokenizer GTest::gtest_main)

add_executable(parserTest parser_test.cc)
target_link_libraries(parserTest PRIVATE parser GTest::gtest_main)

add_executable(evaluatorTest evaluator_test.cc)
target_link_libraries(evaluatorTest PRIVATE evaluator GTest::gtest_main)

add_executable(calculatorTest calculator_test.cc)
target_link_libraries(calculatorTest PRIVATE calculator GTest::gtest_main)

# With below we auto generate .gcda files for coverage
include(GoogleTest)
gtest_discover_tests(tokenizerTest)
gtest_discover_tests(parserTest)
gtest_discover_tests(evaluatorTest)
gtest_discover_tests(calculatorTest)

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
