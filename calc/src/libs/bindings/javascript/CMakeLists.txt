find_program(EMPP_EXECUTABLE em++)
if (NOT EMPP_EXECUTABLE)
    message(WARNING "em++ not found in path.")
    return()
endif()

# em++ calc_js_api.cc ../../core/*.cc -I../../core/include -std=c++23 -O3 --bind -o calculator.js
set(CALC_SOURCE_DIR $<TARGET_PROPERTY:calculator,SOURCE_DIR>)
set(CALC_SOURCES calculator.cc evaluator.cc parser.cc tokenizer.cc)
LIST(TRANSFORM CALC_SOURCES PREPEND ${CALC_SOURCE_DIR}/)
add_custom_command(OUTPUT calculator.wasm
    COMMAND ${EMPP_EXECUTABLE}
    ARGS ${CMAKE_CURRENT_SOURCE_DIR}/calc_js_api.cc 
    ${CALC_SOURCES} -I${CALC_SOURCE_DIR}/include
    $<$<CONFIG:Release>:-O3> -std=c++23 --bind -o calculator.js
    DEPENDS ${CALC_SOURCES}
    COMMENT "Building target calculator.wasm"
    COMMAND_EXPAND_LISTS CODEGEN)

# Making sure the build invokes the custom command above
add_custom_target(calculator_wasm ALL DEPENDS calculator.wasm)
