/* LabVIEW Import Shared Library Wizard declarations.
 * All functions use C / cdecl. Return values and scalar arguments are I32.
 * Allocate arrays before calling; use Array Data Pointer, not handles.
 * image_pixels: width*height U16 elements, contiguous row-major.
 * cropped_pixels: cropped_capacity_pixels U16 elements (NOT bytes).
 * roi_out: 3 I32 elements; metrics_out: 4 DBL elements.
 * FP_GetApiVersion returns 1. See labview_integration.md for status validity.
 */
int FP_GetApiVersion(void);
int FP_AnalyzeFrameU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    unsigned short cropped_pixels[],
    int cropped_capacity_pixels,
    int roi_out[],
    double metrics_out[]);
