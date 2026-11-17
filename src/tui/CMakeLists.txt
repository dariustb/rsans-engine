add_library(rsans_tui
    rsans_file_browser.cpp
    rsans_fonts.cpp
    rsans_project_form.cpp
    rsans_step_runner.cpp
    rsans_title.cpp
    rsans_tui.cpp
    rsans_whisper_models.cpp
)

# The whisper.cpp checkout whose models/download-ggml-model.sh lists and
# downloads Whisper models; models are stored in its models/ folder. Defaults
# to the copy fetched to build whisper.cpp. Overridden at run time by
# $WHISPER_CPP_DIR.
include(FetchContent)
FetchContent_GetProperties(whisper)
set(RSANS_WHISPER_CPP_DIR "" CACHE PATH "whisper.cpp checkout used for Whisper models (default: the fetched copy)")
if(RSANS_WHISPER_CPP_DIR)
    set(whisperCppDir "${RSANS_WHISPER_CPP_DIR}")
else()
    set(whisperCppDir "${whisper_SOURCE_DIR}")
endif()
set_source_files_properties(rsans_whisper_models.cpp PROPERTIES
    COMPILE_DEFINITIONS "RSANS_WHISPER_CPP_DIR=\"${whisperCppDir}\"")

target_include_directories(rsans_tui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)

target_link_libraries(rsans_tui PUBLIC
    rsans_core
    rsans_io
    ftxui::component
    ftxui::dom
    ftxui::screen
    Threads::Threads
    PkgConfig::FONTCONFIG
)
