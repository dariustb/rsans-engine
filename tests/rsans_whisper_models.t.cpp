#include <rsans_whisper_models.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

const char* const sampleScript = R"(#!/bin/sh
models_path="${2:-$default_download_path}"

# Whisper models
models="tiny
tiny.en
base.en
medium
medium-q5_0
small.en-tdrz"

# list available models
)";

const char* const sampleReadme = R"(
| Model               | Disk    | SHA                                        |
| ------------------- | ------- | ------------------------------------------ |
| tiny                | 75 MiB  | `bd577a113a864445d4c299885e0cb97d4ba92b5f` |
| base.en             | 142 MiB | `137c40403d78fd54d454da0f9bd998f78703390c` |
| medium              | 1.5 GiB | `fd9727b6e1217c2f614f9b698455c4ffd82463b4` |
)";

constexpr std::uint64_t mib = 1024ULL * 1024ULL;

class WhisperModelsTest : public ::testing::Test {
  protected:
    fs::path root = fs::temp_directory_path() / ("rsans_whisper_" + std::to_string(::getpid()));

    void SetUp() override {
        fs::remove_all(root);
        fs::create_directories(root / "models");
        std::ofstream(root / "models" / "download-ggml-model.sh") << sampleScript;
        std::ofstream(root / "models" / "README.md") << sampleReadme;
    }
    void TearDown() override { fs::remove_all(root); }

    const WhisperModel& find(const std::vector<WhisperModel>& models, const std::string& name) {
        const auto it = std::find_if(models.begin(), models.end(),
                                     [&](const WhisperModel& m) { return m.name == name; });
        if (it == models.end()) {
            throw std::runtime_error("no model " + name);
        }
        return *it;
    }
};

} // namespace

TEST(WhisperModelParseTest, ParseModelNamesReadsScriptListInOrder) {
    EXPECT_EQ(parseModelNames(sampleScript),
              (std::vector<std::string>{"tiny", "tiny.en", "base.en", "medium", "medium-q5_0", "small.en-tdrz"}));
    EXPECT_TRUE(parseModelNames("#!/bin/sh\necho hi\n").empty());
}

TEST(WhisperModelParseTest, ParseModelSizesReadsTableRows) {
    const auto sizes = parseModelSizes(sampleReadme);
    ASSERT_EQ(sizes.size(), 3u);
    EXPECT_EQ(sizes[0].first, "tiny");
    EXPECT_EQ(sizes[0].second, 75 * mib);
    EXPECT_EQ(sizes[2].first, "medium");
    EXPECT_EQ(sizes[2].second, static_cast<std::uint64_t>(1.5 * 1024 * mib));
}

TEST_F(WhisperModelsTest, ListWhisperModelsMarksInstalledAndSizes) {
    // Given: tiny is downloaded; an empty base.en file is a failed download
    std::ofstream(root / "models" / "ggml-tiny.bin") << "model";
    std::ofstream(root / "models" / "ggml-base.en.bin");

    // When
    const std::vector<WhisperModel> models = listWhisperModels(root.string());

    // Then
    ASSERT_EQ(models.size(), 6u);
    EXPECT_EQ(models[0].name, "tiny");
    EXPECT_EQ(models[0].path, (root / "models" / "ggml-tiny.bin").string());
    EXPECT_TRUE(models[0].installed);
    EXPECT_FALSE(find(models, "base.en").installed);
    EXPECT_EQ(find(models, "base.en").sizeBytes, 142 * mib);
    EXPECT_FALSE(find(models, "base.en").sizeEstimated);

    // Quantized sizes are estimated from the full model; others are unknown
    const WhisperModel& quantized = find(models, "medium-q5_0");
    ASSERT_TRUE(quantized.sizeBytes.has_value());
    EXPECT_TRUE(quantized.sizeEstimated);
    EXPECT_NEAR(static_cast<double>(*quantized.sizeBytes), 0.37 * 1.5 * 1024 * mib, mib);
    EXPECT_FALSE(find(models, "tiny.en").sizeBytes.has_value());
}

TEST_F(WhisperModelsTest, ListWhisperModelsIsEmptyGivenNotACheckout) {
    EXPECT_TRUE(listWhisperModels("").empty());
    EXPECT_TRUE(listWhisperModels((root / "nowhere").string()).empty());
}

TEST(WhisperModelLabelTest, ModelLabelShowsSizeOnlyWhenNotInstalled) {
    WhisperModel model;
    model.name = "medium";
    model.sizeBytes = static_cast<std::uint64_t>(1.5 * 1024 * mib);
    EXPECT_EQ(modelLabel(model), "medium (1.5 GiB)");

    model.sizeEstimated = true;
    EXPECT_EQ(modelLabel(model), "medium (~1.5 GiB)");

    model.installed = true;
    EXPECT_EQ(modelLabel(model), "medium ✓");

    model.installed = false;
    model.sizeBytes.reset();
    EXPECT_EQ(modelLabel(model), "medium (size unknown)");

    EXPECT_EQ(formatSize(75 * mib), "75 MiB");
    EXPECT_EQ(formatSize(2900 * mib), "2.8 GiB");
}

TEST(WhisperModelLabelTest, ParseDownloadProgressReadsCurlAndWget) {
    EXPECT_DOUBLE_EQ(*parseDownloadProgress(" 45  142M   45 64.0M    0     0  10.2M      0  0:00:13"), 0.45);
    EXPECT_DOUBLE_EQ(*parseDownloadProgress("100 1500M  100 1500M    0     0  20.1M      0"), 1.0);
    EXPECT_DOUBLE_EQ(*parseDownloadProgress("ggml-base.bin   45%[=====>      ]  64.0M  10.2MB/s"), 0.45);
    EXPECT_FALSE(parseDownloadProgress("  % Total    % Received % Xferd  Average Speed").has_value());
    EXPECT_FALSE(parseDownloadProgress("Downloading ggml model base.en from 'https://...'").has_value());
}

TEST(WhisperModelDefaultTest, DefaultWhisperDirIsWhisperCheckoutWithModels) {
    // Given: no override
    const char* saved = std::getenv("WHISPER_CPP_DIR");
    const std::string savedValue = saved != nullptr ? saved : "";
    ::unsetenv("WHISPER_CPP_DIR");

    // When: the build's checkout (by default, the one fetched to build whisper.cpp)
    const std::vector<WhisperModel> models = listWhisperModels(defaultWhisperDir());

    // Then: whisper.cpp's own list, with sizes from its README
    const auto has = [&](const std::string& name) {
        return std::any_of(models.begin(), models.end(), [&](const WhisperModel& m) { return m.name == name; });
    };
    EXPECT_TRUE(has("tiny"));
    EXPECT_TRUE(has("base.en"));
    EXPECT_TRUE(has("large-v3-turbo"));
    for (const WhisperModel& model : models) {
        EXPECT_FALSE(model.name.empty());
    }

    // An override wins
    ::setenv("WHISPER_CPP_DIR", "/somewhere/whisper.cpp", 1);
    EXPECT_EQ(defaultWhisperDir(), "/somewhere/whisper.cpp");
    if (saved != nullptr) {
        ::setenv("WHISPER_CPP_DIR", savedValue.c_str(), 1);
    } else {
        ::unsetenv("WHISPER_CPP_DIR");
    }
}

TEST(WhisperModelMainTest, MainWhisperModelsKeepsOneEnglishModelPerSize) {
    // Given: whisper.cpp's full list, with a quantized model installed
    std::vector<WhisperModel> models;
    for (const char* name : {"tiny", "tiny.en", "tiny-q5_1", "base", "base.en", "small", "small.en",
                             "small.en-tdrz", "medium", "medium.en", "medium-q8_0", "large-v1", "large-v2",
                             "large-v2-q5_0", "large-v3", "large-v3-q5_0", "large-v3-turbo",
                             "large-v3-turbo-q5_0"}) {
        WhisperModel model;
        model.name = name;
        models.push_back(model);
    }
    models[2].installed = true;  // tiny-q5_1

    // When
    std::vector<std::string> names;
    for (const std::size_t index : mainWhisperModels(models)) {
        names.push_back(models[index].name);
    }

    // Then
    EXPECT_EQ(names, (std::vector<std::string>{"tiny.en", "tiny-q5_1", "base.en", "small.en", "medium.en",
                                               "large-v3", "large-v3-turbo"}));
}
