cmake_minimum_required(VERSION 3.28)
project(rsans VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

include(CTest)

add_subdirectory(src)
add_subdirectory(tests)

install(TARGETS rsans
    RUNTIME DESTINATION bin
)
