#include "FPAnalyzer.h"
#include <math.h>

/* Compile as C99 and exercise both ROI modes through the actual shared ABI. */
int main(void) {
    unsigned short image[32*32] = {0}, crop[32*32] = {0};
    const int automatic[3] = {0,0,0}, manual[3] = {2,3,24};
    int roi[3] = {1,1,1};
    double metrics[4] = {0,0,0,0};
    int i;
    if (sizeof(int)!=4 || sizeof(unsigned short)!=2 || FP_GetApiVersion()!=2) return 1;
    if (FP_AnalyzeFrameU16(image,32,32,automatic,crop,32*32,roi,metrics)!=FP_NO_FRINGE_REGION) return 2;
    for (i=0;i<3;++i) if (roi[i]!=0) return 3;
    for (i=0;i<4;++i) if (!isnan(metrics[i])) return 4;
    if (FP_AnalyzeFrameU16(image,32,32,manual,crop,32*32,roi,metrics)!=FP_INSUFFICIENT_RINGS) return 5;
    for (i=0;i<3;++i) if (roi[i]!=manual[i]) return 6;
    for (i=0;i<4;++i) if (!isnan(metrics[i])) return 7;
    if (FP_AnalyzeFrameU16WithMethod(image,32,32,manual,99,crop,32*32,roi,metrics)!=FP_INVALID_METHOD) return 8;
    for (i=0;i<3;++i) if (roi[i]!=0) return 9;
    for (i=0;i<4;++i) if (!isnan(metrics[i])) return 10;
    if (FP_AnalyzeFrameU16WithMethod(image,32,32,manual,1,crop,32*32,roi,metrics)!=FP_INSUFFICIENT_RINGS) return 11;
    return 0;
}
