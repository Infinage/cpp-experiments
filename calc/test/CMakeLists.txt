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

include(GoogleTest)
gtest_discover_tests(tokenizerTest)
gtest_discover_tests(parserTest)
gtest_discover_tests(evaluatorTest)
gtest_discover_tests(calculatorTest)
