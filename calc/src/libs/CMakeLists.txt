add_library(tokenizer tokenizer.cc)
target_include_directories(tokenizer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(parser parser.cc)
target_include_directories(parser PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(evaluator evaluator.cc)
target_include_directories(evaluator PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(calculator calculator.cc)
target_link_libraries(calculator PUBLIC tokenizer parser evaluator)
