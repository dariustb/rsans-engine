add_subdirectory(cli)
add_subdirectory(core)
add_subdirectory(io)

add_executable(rsans
    main.cpp
)

target_link_libraries(rsans PUBLIC
    rsans_cli
    rsans_core
    rsans_io
)
