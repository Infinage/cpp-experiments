add_library(tokenizer OBJECT tokenizer.cc)
add_library(parser OBJECT parser.cc)
add_library(evaluator OBJECT evaluator.cc)
add_library(calculator calculator.cc)

foreach(lib IN ITEMS tokenizer parser evaluator calculator)
    add_library(${PROJECT_NAME}::${lib} ALIAS ${lib})
    target_sources(${lib} PUBLIC FILE_SET HEADERS
        BASE_DIRS include FILES "include/calc/${lib}.h")

    # Flags for code coverage with gcov
    if (CMAKE_BUILD_TYPE STREQUAL Debug)
        target_compile_options(${lib} PRIVATE --coverage)
        target_link_libraries(${lib} PUBLIC --coverage)
    endif()
endforeach()

# Calculator needs other libraries to work
target_link_libraries(calculator PRIVATE tokenizer parser evaluator)

# Tie packages to an export target
install(TARGETS calculator tokenizer parser evaluator 
    EXPORT CalcTargets FILE_SET HEADERS)
install(EXPORT CalcTargets NAMESPACE Calc:: 
    DESTINATION ${CMAKE_INSTALL_BINDIR}/cmake/calc)
