include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_rsans)
target_link_libraries(test_rsans
	PUBLIC
		GTest::gtest_main
		rsans_core
)
target_include_directories(test_rsans PUBLIC ./)

target_sources(test_rsans
	PRIVATE
		rsans_speechtotext.t.cpp
)

gtest_discover_tests(test_rsans)