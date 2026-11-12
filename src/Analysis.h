#ifndef FP_ANALYSIS_H
#define FP_ANALYSIS_H

#include <vector>

namespace fp {
struct Roi { int left = 0; int top = 0; int size = 0; };
struct Center { double x = 0; double y = 0; };
struct Profile {
    double step = 1;
    std::vector<double> intensity;
};
struct Measurement { double phase = 0; double finesse = 0; };

bool detectRoi(const unsigned char* pixels, int width, int height, Roi& roi);
bool estimateCenter(const unsigned char* pixels, int size, Center& center);
Profile radialProfile(const unsigned char* pixels, int size, const Center& center);
bool measureProfile(const Profile& profile, Measurement& result);
} // namespace fp
#endif
