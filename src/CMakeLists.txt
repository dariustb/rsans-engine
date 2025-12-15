add_subdirectory(core)

add_executable(rsans
    main.cpp
)

target_link_libraries(rsans PUBLIC
    rsans_core
)
