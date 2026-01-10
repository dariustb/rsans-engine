add_library(rsans_core
    rsans_core.cpp
)

target_include_directories(rsans_core PUBLIC ./)

include(FetchContent)

# Fetch whisper - Skipping extra builds
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_STREAM OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    whisper
    URL https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.8.2.zip
)
FetchContent_MakeAvailable(whisper)

# Fetch Dr Wav
FetchContent_Declare(
    dr_wav
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(dr_wav)

target_link_libraries(rsans_core PUBLIC
    whisper
)

target_include_directories(rsans_core PUBLIC
    ${whisper_SOURCE_DIR}
    ${dr_wav_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
