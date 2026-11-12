#ifndef FP_ANALYZER_H
#define FP_ANALYZER_H

/* FPAnalyzer API v1. All arrays are caller-owned, contiguous and unpadded.
 * Requires 8-bit bytes, 32-bit int and IEEE-754 double on supported platforms.
 * See docs/labview_integration.md for buffer sizes and output validity.
 */
#if defined(_WIN32)
#  define FP_CALL __cdecl
#  if defined(FP_ANALYZER_BUILD)
#    define FP_API __declspec(dllexport)
#  else
#    define FP_API __declspec(dllimport)
#  endif
#else
#  define FP_CALL
#  define FP_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FP_SUCCESS = 0,
    FP_INVALID_ARGUMENT = -1,
    FP_INSUFFICIENT_CAPACITY = -2,
    FP_NO_FRINGE_REGION = -3,
    FP_INSUFFICIENT_RINGS = -4
};

FP_API int FP_CALL FP_GetApiVersion(void);

/* image_pixels: image_width*image_height U8 elements (row-major).
 * cropped_pixels: cropped_capacity_pixels U8 elements; must not overlap input.
 * roi_out: 3 I32 elements [left, top, square_size].
 * metrics_out: 4 DBL elements [phase, center_x, center_y, finesse_proxy].
 * All pointers must be non-null and all storage disjoint. Array lengths are
 * caller obligations; pointers do not convey their allocated lengths.
 * On entry, non-null roi_out is zeroed and non-null metrics_out filled with NaN.
 * Exception: detected overlap returns -1 before any initialization; outputs
 * are untouched and invalid in that case.
 *  0: all outputs valid, only size*size cropped bytes written.
 * -1: ROI zero, metrics NaN, crop untouched.
 * -2: ROI valid (required capacity=size*size), metrics NaN, crop untouched.
 * -3: ROI zero, metrics NaN, crop untouched.
 * -4: if ROI size>0, ROI and crop valid; all metrics NaN. Otherwise no crop.
 * Resource failures are contained and reported as -4 (no metrics).
 * Phase convention: bright peaks satisfy r^2/spacing + phase = integer.
 * Centers are in cropped-image pixel coordinates, including fractional parts.
 */
FP_API int FP_CALL FP_AnalyzeFrameU8(
    const unsigned char image_pixels[], int image_width, int image_height,
    unsigned char cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[3], double metrics_out[4]);

/* Native U16 counterpart with the same status/geometry/metric contract above;
 * on success size*size U16 elements (2*size*size bytes) are written.
 * unsigned short is 16 bits. Capacities/counts are U16 ELEMENTS, not bytes;
 * each input/output pixel occupies 2 bytes. Crop values are copied exactly.
 * Analysis uses the original precision; no U8 conversion is performed.
 * API version remains 1: this is an additive, backward-compatible entry point.
 */
FP_API int FP_CALL FP_AnalyzeFrameU16(
    const unsigned short image_pixels[], int image_width, int image_height,
    unsigned short cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[], double metrics_out[]);

#ifdef __cplusplus
}
#endif
#endif
