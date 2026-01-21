add_library(rsans_core
    rsans_ass.cpp
    rsans_data.cpp
    rsans_export.cpp
    rsans_rhyme.cpp
    rsans_whisper.cpp
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

# Fetch nlohmann's json (https://github.com/nlohmann/json)
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.zip
)
FetchContent_MakeAvailable(nlohmann_json)

target_link_libraries(rsans_core PUBLIC
    rsans_io
    whisper
    nlohmann_json::nlohmann_json
)

target_include_directories(rsans_core PUBLIC
    ${whisper_SOURCE_DIR}
    ${dr_wav_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
