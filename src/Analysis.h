#ifndef FP_ANALYSIS_H
#define FP_ANALYSIS_H

#include <vector>
#include <cstddef>

namespace fp {
struct Roi { int left = 0; int top = 0; int size = 0; };
struct Center { double x = 0; double y = 0; };
struct Profile {
    double step = 1;
    std::vector<double> intensity;
};
struct Measurement { double phase = 0; double finesse = 0; };

// Analysis-only affine intensity view. Samples remain double precision;
// caller-owned pixels and the returned crop are never rescaled.
template<typename Pixel> struct PixelView {
    const Pixel* pixels;
    double offset = 0, gain = 1;
    double operator[](std::size_t i) const {
        return (static_cast<double>(pixels[i])-offset)*gain;
    }
};
inline PixelView<unsigned char> analysisView(const unsigned char* p, std::size_t) {
    return {p}; // Keep legacy U8 intensity thresholds and behavior.
}
PixelView<unsigned short> analysisView(const unsigned short* p, std::size_t count);
template<typename Pixel> bool detectRoi(PixelView<Pixel> pixels, int width, int height, Roi& roi);
template<typename Pixel> bool estimateCenter(PixelView<Pixel> pixels, int size, Center& center);
template<typename Pixel> Profile radialProfile(PixelView<Pixel> pixels, int size, const Center& center);

bool detectRoi(const unsigned char* pixels, int width, int height, Roi& roi);
bool estimateCenter(const unsigned char* pixels, int size, Center& center);
Profile radialProfile(const unsigned char* pixels, int size, const Center& center);
bool measureProfile(const Profile& profile, Measurement& result);
} // namespace fp
#endif
