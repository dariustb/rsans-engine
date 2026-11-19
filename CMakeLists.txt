cmake_minimum_required(VERSION 3.28)
project(rsans VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Fetch CMUdict from GitHub
FetchContent_Declare(
    cmudict
    GIT_REPOSITORY https://github.com/cmusphinx/cmudict.git
    GIT_TAG master
)
FetchContent_MakeAvailable(cmudict)

# Fetch FTXUI (terminal UI framework) - Skipping extra builds
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ftxui
    URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.0.0.zip
)
FetchContent_MakeAvailable(ftxui)

# Create data directory in project root
set(DATA_DIR ${CMAKE_SOURCE_DIR}/data)
file(MAKE_DIRECTORY ${DATA_DIR})

# Copy CMUdict files to root/data/ for dev work
file(COPY ${cmudict_SOURCE_DIR}/cmudict.dict
     DESTINATION ${DATA_DIR})
file(COPY ${cmudict_SOURCE_DIR}/LICENSE
     DESTINATION ${DATA_DIR})

include(CTest)

add_subdirectory(src)
add_subdirectory(tests)

# Run clang-tidy on project targets only (not on fetched dependencies)
set_target_properties(rsans rsans_cli rsans_core rsans_io rsans_tui test_rsans
    PROPERTIES CXX_CLANG_TIDY "clang-tidy"
)

install(TARGETS rsans
    RUNTIME DESTINATION bin
)
