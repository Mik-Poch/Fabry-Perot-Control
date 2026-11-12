#ifndef FP_TEST_SYNTHETIC_H
#define FP_TEST_SYNTHETIC_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

namespace synthetic {
constexpr double pi = 3.14159265358979323846;
struct Parameters {
    int width = 640, height = 480;
    double cx = 319.3, cy = 239.7, radius = 185;
    double spacing = 4800, phase = 0.27, airy = 24;
    double amplitude = 200, background = 4, noise = 0;
    unsigned seed = 42;
};
inline std::vector<unsigned char> frame(const Parameters& p) {
    std::vector<unsigned char> image(static_cast<std::size_t>(p.width)*p.height);
    std::mt19937 rng(p.seed);
    std::normal_distribution<double> noise(0,1);
    for (int y = 0; y < p.height; ++y) for (int x = 0; x < p.width; ++x) {
        const double dx = x-p.cx,dy = y-p.cy, q = dx*dx+dy*dy;
        const double s = std::sin(pi*(q/p.spacing+p.phase));
        // Flat illumination in the useful region and a smooth finite aperture.
        const double taper = std::clamp((p.radius-std::sqrt(q))/12.0,0.0,1.0);
        const double value = p.background+taper*p.amplitude/(1+p.airy*s*s)+p.noise*noise(rng);
        image[static_cast<std::size_t>(y)*p.width+x] = static_cast<unsigned char>(
            std::clamp(std::lround(value),0L,255L));
    }
    return image;
}
inline double phaseError(double a, double b) {
    double d = std::abs(a-b);
    d -= std::floor(d);
    return std::min(d,1-d);
}
}
#endif
