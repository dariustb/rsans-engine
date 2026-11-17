add_library(rsans_tui
    rsans_project_form.cpp
    rsans_step_runner.cpp
    rsans_tui.cpp
)

target_include_directories(rsans_tui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Threads REQUIRED)

target_link_libraries(rsans_tui PUBLIC
    rsans_core
    rsans_io
    ftxui::component
    ftxui::dom
    ftxui::screen
    Threads::Threads
)
