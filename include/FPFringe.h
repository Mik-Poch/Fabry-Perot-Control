/* FPFringe API v3, C/cdecl. Caller-owned, disjoint, contiguous U16/I32/DBL
 * buffers. Unsized arrays are pointers; allocate documented lengths in LabVIEW.
 * Detailed status, coordinate and CLFN contracts: docs/fpfringe.md. */
enum {
    FPF_SUCCESS = 0,
    FPF_INVALID_ARGUMENT = -1,
    FPF_INSUFFICIENT_CAPACITY = -2,
    FPF_NO_FRINGE_REGION = -3,
    FPF_UNRELIABLE_MEASUREMENT = -4,
    FPF_INVALID_METHOD = -5
};

int FPF_GetApiVersion(void);

/* Centered square U16 crop: requested size 0=auto, >0=exact.
 * roi_pixels has roi_capacity_pixels U16 elements; no DLL allocation.
 * roi_out I32[3] = [left, top, side]; center_out DBL[2] = full-frame center.
 * On -2, geometry/center are valid and roi_pixels remains unchanged. */
int FPF_FindCenteredRoiU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int requested_roi_size,
    unsigned short roi_pixels[],
    int roi_capacity_pixels,
    int roi_out[],
    double center_out[]);

/* Analyze the exact supplied rectangle; methods 0=A, 1=B, 2=C, 3=D. */
int FPF_AnalyzeU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int analysis_method,
    double metrics_out[]);

/* Analyze the exact supplied rectangle; 16 sectors, four diagnostics each. */
int FPF_AnalyzeAsymmetryU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    double asymmetry_out[],
    double sector_out[]);
