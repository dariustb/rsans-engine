add_library(rsans_tui
    rsans_audio_player.cpp
    rsans_file_browser.cpp
    rsans_fonts.cpp
    rsans_project_form.cpp
    rsans_settings.cpp
    rsans_step_runner.cpp
    rsans_ticker.cpp
    rsans_title.cpp
    rsans_tui.cpp
    rsans_whisper_models.cpp
)

target_include_directories(rsans_tui PUBLIC ./
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)

include(FetchContent)

# Fetch miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio
    GIT_TAG        master
)
FetchContent_MakeAvailable(miniaudio)

# Fetch Whisper download-ggml-model.sh lists
# (Overridden at run time by $WHISPER_CPP_DIR.)
FetchContent_GetProperties(whisper)
set(RSANS_WHISPER_CPP_DIR "" CACHE PATH "whisper.cpp checkout used for Whisper models (default: the fetched copy)")
if(RSANS_WHISPER_CPP_DIR)
    set(whisperCppDir "${RSANS_WHISPER_CPP_DIR}")
else()
    set(whisperCppDir "${whisper_SOURCE_DIR}")
endif()
set_source_files_properties(rsans_whisper_models.cpp PROPERTIES
    COMPILE_DEFINITIONS "RSANS_WHISPER_CPP_DIR=\"${whisperCppDir}\"")

target_link_libraries(rsans_tui PUBLIC
    rsans_core
    rsans_io
    ftxui::component
    ftxui::dom
    ftxui::screen
    Threads::Threads
    PkgConfig::FONTCONFIG
)

# Audio output, used only inside rsans_audio_player.cpp
target_link_libraries(rsans_tui PRIVATE miniaudio)
