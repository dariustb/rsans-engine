include(FetchContent)

# Fetch Gtest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_rsans)
target_link_libraries(test_rsans
	PUBLIC
		GTest::gtest_main
		rsans_cli
		rsans_core
		rsans_tui
)
target_include_directories(test_rsans PUBLIC ./)

target_sources(test_rsans
	PRIVATE
		rsans_args.t.cpp
		rsans_ass.t.cpp
		rsans_data.t.cpp
		rsans_export.t.cpp
		rsans_file_browser.t.cpp
		rsans_fonts.t.cpp
		rsans_process.t.cpp
		rsans_rhyme.t.cpp
		rsans_settings.t.cpp
		rsans_tui.t.cpp
		rsans_whisper_models.t.cpp
		rsans_whisper.t.cpp
)

gtest_discover_tests(test_rsans)
