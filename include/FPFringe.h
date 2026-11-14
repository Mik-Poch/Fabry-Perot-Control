/* FPFringe API v1, C/cdecl. Caller-owned, disjoint, contiguous U16/I32/DBL
 * buffers. Bounds below document required allocations, not LabVIEW allocation.
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

/* Geometry only: requested size 0 selects automatic, positive selects exact. */
int FPF_FindCenteredRoiU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int requested_roi_size,
    int roi_out[4],
    double center_out[2]);

/* Successful screening returns status 0 and an I32 Boolean (0 or 1). */
int FPF_CheckFringesU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int *analyzable_out);

/* Analyze the exact supplied rectangle; methods 0=A, 1=B, 2=C, 3=D. */
int FPF_AnalyzeU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int analysis_method,
    double metrics_out[4]);

/* Analyze the exact supplied rectangle; 16 sectors, four diagnostics each. */
int FPF_AnalyzeAsymmetryU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    double asymmetry_out[16],
    double sector_out[64]);
