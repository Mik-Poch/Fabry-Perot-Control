/* Plain C99 caller: no C++ wrapper, private macros or implementation headers.
 * Array parameters (sized or unsized) must retain these pointer ABI types. */
#include "FPAnalyzer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "C ABI check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef int (*Version)(void);
typedef int (*Frame)(const unsigned short*, int, int, const int*,
                     unsigned short*, int, int*, double*);
typedef int (*Method)(const unsigned short*, int, int, const int*, int,
                      unsigned short*, int, int*, double*);
typedef int (*Asymmetry)(const unsigned short*, int, int, const int*,
                         int*, double*, double*);

int main(void) {
    Version version = FP_GetApiVersion;
    Frame frame = FP_AnalyzeFrameU16;
    Method method = FP_AnalyzeFrameU16WithMethod;
    Asymmetry asymmetry = FP_AnalyzeFringeAsymmetryU16;
    const int size = 320, selected[3] = {0, 0, 320};
    const double sentinel = -1234567.0;
    unsigned short* image;
    unsigned short blank[32*32] = {0}, crop[32*32];
    const int automatic[3] = {0, 0, 0};
    int old_roi[3];
    double old_metrics[4];
    struct { int before, values[3], after; } roi;
    struct { double before, values[16], after; } metrics;
    struct { double before, values[64], after; } sectors;
    int x, y, i;

    CHECK(sizeof(int) == 4 && sizeof(unsigned short) == 2 && sizeof(double) == 8);
    CHECK(version() == 2);
    CHECK(frame(blank,32,32,automatic,crop,32*32,old_roi,old_metrics) == FP_NO_FRINGE_REGION);
    CHECK(method(blank,32,32,automatic,1,crop,32*32,old_roi,old_metrics) == FP_NO_FRINGE_REGION);
    image = (unsigned short*)malloc((size_t)size*size*sizeof(*image));
    CHECK(image != NULL);
    for (y=0; y<size; ++y) for (x=0; x<size; ++x) {
        const double dx=x-153.3, dy=y-164.7;
        const double s=sin(3.14159265358979323846*((dx*dx+dy*dy)/3600.0+0.27));
        image[(size_t)y*size+x]=(unsigned short)lround(1000.0+20000.0/(1.0+5.0*s*s));
    }
    roi.before=123; roi.after=456;
    metrics.before=123; metrics.after=456;
    sectors.before=123; sectors.after=456;
    for (i=0; i<3; ++i) roi.values[i]=-1234567;
    for (i=0; i<16; ++i) metrics.values[i]=sentinel;
    for (i=0; i<64; ++i) sectors.values[i]=sentinel;
    i=asymmetry(image,size,size,selected,roi.values,metrics.values,sectors.values);
    free(image);
    CHECK(i == FP_SUCCESS);
    CHECK(roi.before == 123 && roi.after == 456);
    CHECK(metrics.before == 123 && metrics.after == 456);
    CHECK(sectors.before == 123 && sectors.after == 456);
    for (i=0; i<3; ++i) CHECK(roi.values[i] == selected[i]);
    for (i=0; i<16; ++i) CHECK(metrics.values[i] != sentinel);
    CHECK(fabs(metrics.values[1]-153.3) < 0.65 && fabs(metrics.values[2]-164.7) < 0.65);
    CHECK(metrics.values[0] >= 0 && metrics.values[0] < 1 && metrics.values[13] >= 3);
    CHECK(metrics.values[14] == 16);
    for (i=0; i<16; ++i) {
        CHECK(isfinite(sectors.values[4*i]) && sectors.values[4*i] > 0);
        CHECK(isfinite(sectors.values[4*i+1]) && sectors.values[4*i+1] > 0);
        CHECK(sectors.values[4*i+2] >= 2 && sectors.values[4*i+3] == 1);
    }
    puts("PASS plain C: four public types, version 2, successful asymmetry call, exact 3/16/64 outputs and canaries");
    return 0;
}
