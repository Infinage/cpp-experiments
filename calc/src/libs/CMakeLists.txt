add_library(tokenizer tokenizer.cc)
target_include_directories(tokenizer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(parser parser.cc)
target_include_directories(parser PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(evaluator evaluator.cc)
target_include_directories(evaluator PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(calculator calculator.cc)
target_link_libraries(calculator PUBLIC tokenizer parser evaluator)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    foreach(lib IN ITEMS tokenizer parser evaluator calculator)
        target_compile_options(${lib} PRIVATE --coverage)
        target_link_options(${lib} PUBLIC --coverage)
    endforeach()
endif()
