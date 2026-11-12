#include "FPAnalyzer.h"
#include <math.h>

/* Compile this translation unit as C, then exercise the real shared-library ABI. */
int main(void) {
    unsigned char image[32*32] = {0}, crop[32*32] = {0};
    int roi[3] = {1,1,1};
    double metrics[4] = {0,0,0,0};
    int i;
    if (sizeof(int) != 4 || FP_GetApiVersion() != 1) return 1;
    if (FP_AnalyzeFrameU8(image,32,32,crop,32*32,roi,metrics) != FP_NO_FRINGE_REGION) return 2;
    for (i=0;i<3;++i) if (roi[i] != 0) return 3;
    for (i=0;i<4;++i) if (!isnan(metrics[i])) return 4;
    return 0;
}
