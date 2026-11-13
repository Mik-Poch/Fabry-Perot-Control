/* FPAnalyzer API v2. Permanent public header for FPAnalyzer.dll.
 * Plain C declarations for the LabVIEW Import Shared Library Wizard.
 * All functions use C linkage and C / cdecl. C++ callers wrap the include in
 * extern "C" in their own source. No special preprocessor definitions needed.
 * Requires 8-bit bytes, 16-bit unsigned short, 32-bit int, IEEE-754 double.
 * Breaking change from v1: U8 removed; roi_in added to FP_AnalyzeFrameU16.
 */
enum {
    FP_SUCCESS = 0,
    FP_INVALID_ARGUMENT = -1,
    FP_INSUFFICIENT_CAPACITY = -2,
    FP_NO_FRINGE_REGION = -3,
    FP_INSUFFICIENT_RINGS = -4
};

int FP_GetApiVersion(void);

/* All arrays are caller-owned, contiguous, unpadded and mutually disjoint.
 * image_pixels: image_width*image_height U16 elements, row-major.
 * Positive dimensions and their product must fit signed I32.
 * roi_in: always 3 allocated I32 elements. [0,0,0] selects automatic ROI;
 * otherwise [left_x,top_y,square_size] must have nonnegative coordinates,
 * positive size and fit entirely inside the image. Valid manual ROI is exact.
 * cropped_pixels: cropped_capacity_pixels U16 ELEMENTS, not bytes.
 * roi_out: exactly 3 I32 elements [left_x,top_y,square_size].
 * metrics_out: exactly 4 DBL elements [phase,center_x,center_y,finesse_proxy].
 * All pointers must be non-null; actual array lengths are caller obligations.
 * A pointer does not convey its length. Never pass an empty LabVIEW array.
 * On entry non-null roi_out is zeroed and metrics_out filled with NaN.
 * Exception: detected overlap returns -1 before initialization; outputs are
 * untouched and invalid. Source/ROI inputs are never modified.
 *  0: all outputs valid; exactly size*size original U16 pixels copied.
 * -1: invalid argument/ROI; ROI zero, crop untouched, metrics NaN.
 * -2: insufficient capacity; ROI valid (required=size*size), crop untouched,
 *     metrics NaN. Capacity is checked before any crop write.
 * -3: automatic mode found no fringe region; ROI zero, crop untouched,
 *     metrics NaN. Manual mode never returns -3.
 * -4: unreliable measurement/resource failure; if ROI size>0, ROI and crop
 *     are valid, metrics NaN. Otherwise no crop was produced.
 * Crop tail is untouched. No exception or ownership transfer crosses the ABI.
 * Both modes normalize/analyze only the crop; auto also scans the full frame
 * to detect ROI. Centers are subpixel coordinates relative to the crop.
 * Phase convention: bright peaks satisfy r^2/spacing + phase = integer.
 * See docs/labview_integration.md for the four-input wrapper and wiring.
 */
int FP_AnalyzeFrameU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    const int roi_in[3],
    unsigned short cropped_pixels[],
    int cropped_capacity_pixels,
    int roi_out[3],
    double metrics_out[4]);
