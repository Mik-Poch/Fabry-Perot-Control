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
struct Measurement { double phase = 0; double finesse = 0; double spacing = 0; };

// Analysis-only affine intensity view. Samples remain double precision;
// caller-owned pixels and the returned crop are never rescaled.
struct PixelView {
    const unsigned short* pixels;
    double offset = 0, gain = 1;
    double operator[](std::size_t i) const {
        return (static_cast<double>(pixels[i])-offset)*gain;
    }
};
PixelView analysisView(const unsigned short* p, std::size_t count);
bool detectRoi(PixelView pixels, int width, int height, Roi& roi);
bool estimateCenter(PixelView pixels, int size, Center& center);
Profile radialProfile(PixelView pixels, int size, const Center& center, int stride = 1);

struct ProfileFit { std::vector<double> peakQ; };
bool measureProfile(const Profile& profile, Measurement& result, ProfileFit* fit = nullptr);
} // namespace fp
#endif
