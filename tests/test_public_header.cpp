extern "C" {
#include "FPAnalyzer.h"
}

#include <cmath>
#include <type_traits>

// These checks use only the public declarations: no private build definitions.
using Version = int (*)();
using AnalyzeU16 = int (*)(const unsigned short*, int, int, const int*, unsigned short*, int, int*, double*);
static_assert(std::is_same_v<decltype(&FP_GetApiVersion), Version>);
static_assert(std::is_same_v<decltype(&FP_AnalyzeFrameU16), AnalyzeU16>);

int main() {
    unsigned short image[32*32] = {}, crop[32*32];
    const int automatic[3] = {0,0,0};
    int roi[3] = {1,1,1};
    double metrics[4] = {};
    if (sizeof(unsigned short)!=2 || FP_GetApiVersion()!=2) return 1;
    for (auto& pixel : crop) pixel=65535;
    if (FP_AnalyzeFrameU16(image,32,32,automatic,crop,32*32,roi,metrics)!=FP_NO_FRINGE_REGION) return 2;
    for (int value : roi) if (value!=0) return 3;
    for (double value : metrics) if (!std::isnan(value)) return 4;
    for (auto pixel : crop) if (pixel!=65535) return 5;
    return 0;
}
