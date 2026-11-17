add_library(rsans_tui
    rsans_file_browser.cpp
    rsans_fonts.cpp
    rsans_project_form.cpp
    rsans_step_runner.cpp
    rsans_tui.cpp
)

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
