#ifndef INCLUDED_RSANS_TEST_UTILS
#define INCLUDED_RSANS_TEST_UTILS

#include <rsans_process.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Writes a silent 16-bit PCM WAV file to the temp directory and returns its path
inline std::string writeTestWav(const std::string& name,
                                uint16_t channels,
                                uint32_t sampleRate,
                                uint32_t durationMs = 100) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    const uint32_t frameCount = static_cast<uint32_t>(
        static_cast<uint64_t>(sampleRate) * durationMs / 1000);
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = channels * bitsPerSample / 8;
    const uint32_t dataSize = frameCount * blockAlign;

    std::ofstream out(path, std::ios::binary);
    const auto write32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    const auto write16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };

    out.write("RIFF", 4);
    write32(36 + dataSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write32(16);                       // fmt chunk size
    write16(1);                        // PCM
    write16(channels);
    write32(sampleRate);
    write32(sampleRate * blockAlign);  // byte rate
    write16(blockAlign);
    write16(bitsPerSample);
    out.write("data", 4);
    write32(dataSize);
    const std::vector<char> silence(dataSize, 0);
    out.write(silence.data(), static_cast<std::streamsize>(silence.size()));

    return path.string();
}

// True if an ffmpeg binary can be run from PATH
inline bool ffmpegAvailable() {
    try {
        return runProcess({"ffmpeg", "-version"}) == 0;
    } catch (const std::runtime_error&) {
        return false;
    }
}

// Writes a 1x1 24-bit BMP image to path
inline void writeTestBmp(const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    const auto write32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    const auto write16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };

    out.write("BM", 2);
    write32(58);       // file size: 14 + 40 header bytes + 4 pixel bytes
    write32(0);        // reserved
    write32(54);       // offset to pixels
    write32(40);       // BITMAPINFOHEADER size
    write32(1);        // width
    write32(1);        // height
    write16(1);        // planes
    write16(24);       // bits per pixel
    write32(0);        // no compression
    write32(4);        // pixel data size (row padded to 4 bytes)
    write32(2835);     // 72 DPI
    write32(2835);
    write32(0);        // palette colors
    write32(0);        // important colors
    const char pixel[4] = {0, 0, 0x7f, 0};  // one red-ish pixel + padding
    out.write(pixel, 4);
}

#endif
