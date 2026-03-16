cmake_minimum_required(VERSION 3.30)
project(cgit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZLIB REQUIRED)

add_executable(cgit)
target_link_libraries(cgit ZLIB::ZLIB)
target_sources(cgit 
    PRIVATE
        main.cpp 
        src/CommandHandler.cpp
        src/GitIgnore.cpp
        src/GitIndex.cpp
        src/GitObjects.cpp
        src/GitPack.cpp
        src/GitRepository.cpp
        src/utils.cpp         

    PRIVATE FILE_SET HEADERS BASE_DIRS include
    FILES 
        include/CommandHandler.hpp
        include/GitIgnore.hpp
        include/GitIndex.hpp
        include/GitObjects.hpp
        include/GitPack.hpp
        include/GitRepository.hpp
        include/utils.hpp
)
