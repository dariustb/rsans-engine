add_library(rsans_io
    rsans_io.cpp
)

target_include_directories(rsans_io PUBLIC ./
    ${CMAKE_CURRENT_SOURCE_DIR}
)
