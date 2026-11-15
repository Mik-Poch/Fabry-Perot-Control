#ifndef FP_TEST_SYNTHETIC_H
#define FP_TEST_SYNTHETIC_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>
#include <limits>

namespace synthetic {
constexpr double pi = 3.14159265358979323846;
struct Parameters {
    int width = 640, height = 480;
    double cx = 319.3, cy = 239.7, radius = 185;
    double spacing = 4800, phase = 0.27, airy = 24;
    double amplitude = 200, background = 4, noise = 0;
    unsigned seed = 42;
    double backgroundX=0, backgroundY=0; // Intensity change across one frame.
    double illuminationX=0, illuminationY=0; // Fractional amplitude slopes.
    double ellipticity=0; // q=(1+e)*dx^2+(1-e)*dy^2; model mismatch.
};
template<typename Pixel> inline std::vector<Pixel> frameTyped(const Parameters& p) {
    std::vector<Pixel> image(static_cast<std::size_t>(p.width)*p.height);
    std::mt19937 rng(p.seed);
    std::normal_distribution<double> noise(0,1);
    for (int y = 0; y < p.height; ++y) for (int x = 0; x < p.width; ++x) {
        const double dx = x-p.cx,dy = y-p.cy, q = (1+p.ellipticity)*dx*dx+(1-p.ellipticity)*dy*dy;
        const double s = std::sin(pi*(q/p.spacing+p.phase));
        // Flat illumination in the useful region and a smooth finite aperture.
        const double taper = std::clamp((p.radius-std::sqrt(q))/12.0,0.0,1.0);
        const double xx=(x-p.cx)/p.width, yy=(y-p.cy)/p.height;
        const double value = p.background+p.backgroundX*xx+p.backgroundY*yy+
            taper*p.amplitude*(1+p.illuminationX*xx+p.illuminationY*yy)/(1+p.airy*s*s)+p.noise*noise(rng);
        image[static_cast<std::size_t>(y)*p.width+x] = static_cast<Pixel>(
            std::clamp(std::lround(value),0L,static_cast<long>(std::numeric_limits<Pixel>::max())));
    }
    return image;
}
inline std::vector<unsigned short> frame(const Parameters& p) {
    return frameTyped<unsigned short>(p);
}
inline double phaseError(double a, double b) {
    double d = std::abs(a-b);
    d -= std::floor(d);
    return std::min(d,1-d);
}
}
#endif
