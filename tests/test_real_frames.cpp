extern "C" {
#include "FPAnalyzer.h"
}
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Dependency-free optional integration input: 8-bit binary PGM (P5).
namespace {
std::string token(std::istream& stream) {
    std::string text;
    while (stream >> text) {
        if (text[0] != '#') return text;
        stream.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }
    throw std::runtime_error("truncated PGM header");
}
}
int main(int argc,char** argv) {
    if (argc != 2) return 1;
    try {
        const std::filesystem::path directory(argv[1]);
        int count = 0,failed = 0;
        if (!std::filesystem::exists(directory)) return 77;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() != ".pgm") continue;
            ++count;
            std::ifstream file(entry.path(),std::ios::binary);
            if (token(file) != "P5") throw std::runtime_error("expected binary P5 PGM");
            const int w = std::stoi(token(file)),h = std::stoi(token(file));
            if (token(file) != "255" || w <= 0 || h <= 0 ||
                static_cast<long long>(w)*h > std::numeric_limits<int>::max())
                throw std::runtime_error("unsupported PGM dimensions/depth");
            char delimiter = 0; file.get(delimiter);
            if (delimiter == '\r' && file.peek() == '\n') file.get();
            std::vector<unsigned char> pixels(static_cast<std::size_t>(w)*h),crop(pixels.size());
            if (!file.read(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size())))
                throw std::runtime_error("truncated PGM pixels");
            int roi[3]; double metrics[4];
            const int status = FP_AnalyzeFrameU8(pixels.data(),w,h,crop.data(),static_cast<int>(crop.size()),roi,metrics);
            std::cout << entry.path().filename() << " status=" << status;
            for (double value : metrics) std::cout << ' ' << value;
            std::cout << '\n';
            // Optional positive examples must be analyzable. This checks pipeline
            // execution only, not accuracy without independently labeled truth.
            if (status != 0) ++failed;
        }
        if (!count) { std::cout << "SKIP: no real .pgm frames supplied\n"; return 77; }
        std::cout << "real frames=" << count << " failed=" << failed << '\n';
        return failed ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
