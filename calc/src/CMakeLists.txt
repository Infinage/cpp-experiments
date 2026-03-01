add_library(tokenizer tokenizer.cc)
target_include_directories(tokenizer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(parser parser.cc)
target_include_directories(parser PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
