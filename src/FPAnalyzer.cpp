extern "C" {
#include "FPAnalyzer.h"
}
#include "Pipeline.h"

// API version identifies ABI compatibility, not the feature generation.
extern "C" FP_INTERNAL_EXPORT int FP_INTERNAL_CALL FP_GetApiVersion(void) { return 2; }

extern "C" FP_INTERNAL_EXPORT int FP_INTERNAL_CALL FP_AnalyzeFrameU16(
    const unsigned short image_pixels[], int image_width, int image_height,
    const int roi_in[3], unsigned short cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[3], double metrics_out[4]) {
    return fp::analyzeFrame(image_pixels,image_width,image_height,roi_in,1,
                           cropped_pixels,cropped_capacity_pixels,roi_out,metrics_out);
}

extern "C" FP_INTERNAL_EXPORT int FP_INTERNAL_CALL FP_AnalyzeFrameU16WithMethod(
    const unsigned short image_pixels[], int image_width, int image_height,
    const int roi_in[3], int analysis_method, unsigned short cropped_pixels[],
    int cropped_capacity_pixels, int roi_out[3], double metrics_out[4]) {
    return fp::analyzeFrame(image_pixels,image_width,image_height,roi_in,analysis_method,
                           cropped_pixels,cropped_capacity_pixels,roi_out,metrics_out);
}
