#include "FPAnalyzer_LabVIEW.h"
#include <math.h>

/* Compile only the Wizard header as C and call the compiled cdecl exports. */
int main(void) {
    unsigned short image[32*32] = {0}, crop[32*32];
    int roi[3] = {1,1,1};
    double metrics[4] = {0,0,0,0};
    int i;
    if (sizeof(unsigned short)!=2 || FP_GetApiVersion()!=1) return 1;
    for (i=0;i<32*32;++i) crop[i]=65535;
    if (FP_AnalyzeFrameU16(image,32,32,crop,32*32,roi,metrics)!=-3) return 2;
    for (i=0;i<3;++i) if (roi[i]!=0) return 3;
    for (i=0;i<4;++i) if (!isnan(metrics[i])) return 4;
    for (i=0;i<32*32;++i) if (crop[i]!=65535) return 5;
    return 0;
}
