// Gives each test process a temp folder of its own.
//
// ctest runs every test in its own process, several at once with -j, and
// many tests write files with fixed names to the temp folder (test audio,
// fixture folders). Pointing TMPDIR at a fresh folder per process keeps those
// processes apart: std::filesystem::temp_directory_path(), the code under
// test, and the programs it runs (ffmpeg) all read TMPDIR. The folder is
// removed when the tests finish.

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <stdlib.h>  // mkdtemp, setenv

namespace {

class TempFolderEnvironment : public ::testing::Environment {
  private:
    std::filesystem::path d_folder;

  public:
    void SetUp() override {
        // Inside the usual temp folder, before TMPDIR is changed
        std::string pattern = (std::filesystem::temp_directory_path() / "rsans-test-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        if (mkdtemp(buffer.data()) == nullptr) {
            throw std::runtime_error("Can't create a temp folder for the tests from " + pattern);
        }
        d_folder = buffer.data();
        setenv("TMPDIR", d_folder.c_str(), 1);
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(d_folder, ignored);
    }
};

// Registered before main runs; gtest_main then sets it up before any test
// NOLINTNEXTLINE(cert-err58-cpp): registering an environment is the intended use
const ::testing::Environment* const tempFolder =
    ::testing::AddGlobalTestEnvironment(new TempFolderEnvironment);  // gtest owns and deletes it

} // namespace
