add_library(rsans_cli
    rsans_args.cpp
)
target_include_directories(rsans_cli PUBLIC ./)

include(FetchContent)

# Fetch CLI11
FetchContent_Declare(
    cli11
    URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.6.1.zip
)
FetchContent_MakeAvailable(cli11)

target_link_libraries(rsans_cli PUBLIC
    CLI11::CLI11
)

target_include_directories(rsans_cli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
