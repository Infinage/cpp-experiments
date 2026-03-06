add_library(token INTERFACE)
target_sources(token INTERFACE FILE_SET HEADERS
    BASE_DIRS include FILES "include/calc/token.h")

add_library(tokenizer OBJECT tokenizer.cc)
add_library(parser OBJECT parser.cc)
add_library(evaluator OBJECT evaluator.cc)
add_library(calculator calculator.cc)

foreach(lib IN ITEMS tokenizer parser evaluator calculator)
    target_sources(${lib} PUBLIC FILE_SET HEADERS
        BASE_DIRS include FILES "include/calc/${lib}.h")

    # Every object needs to link with token interface
    target_link_libraries(${lib} PUBLIC token)

    # Flags for code coverage with gcov
    target_compile_options(${lib} PRIVATE $<$<CONFIG:Debug>:--coverage>)
    target_link_options(${lib} PUBLIC $<$<CONFIG:Debug>:--coverage>)
endforeach()

# Calculator needs other libraries to work
add_library(${PROJECT_NAME}::calculator ALIAS calculator)
target_link_libraries(calculator PRIVATE tokenizer parser evaluator)

# For generating C bindings as shared lib
set_target_properties(calculator tokenizer parser evaluator 
    PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Tie packages to an export target
install(TARGETS calculator tokenizer parser evaluator token 
    EXPORT CalcTargets FILE_SET HEADERS)
