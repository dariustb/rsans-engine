#include <rsans_io.h>

#include <fstream>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <string>

std::string readFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Failed to open: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::string& data) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to write: " + path);
    }
    ofs.write(data.data(), data.size());
}
