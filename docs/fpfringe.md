# FPFringe API 2

FPFringe is a separate C++17 analysis library for Windows x86 (32-bit LabVIEW
on Windows 11 x64) and Windows x64. The permanent files are **FPFringe.dll** and
**FPFringe.h**. FPAnalyzer.dll/FPAnalyzer.h and their API 2 exports remain
available with unchanged public declarations and legacy ROI semantics. Neither
DLL loads the other: both link the internal static analysis core. No OpenCV,
Python, GPU, LabVIEW runtime, camera driver or external fitting library is
required by the DLL. Python tools are optional validation clients.

## Four public C/cdecl functions

```c
int FPF_GetApiVersion(void);

int FPF_FindCenteredRoiU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int requested_roi_size,
    int roi_out[],
    double center_out[]);

int FPF_AnalyzeU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    int analysis_method,
    double metrics_out[]);

int FPF_AnalyzeAsymmetryU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    double asymmetry_out[],
    double sector_out[]);
```

Exports have these exact undecorated names. FPF_GetApiVersion returns **2**;
FP_GetApiVersion in the old DLL still returns **2**, independently.
FPFringe API 2 removes the standalone checker from API 1. Resolve only the four
exports above and use each analysis call's return status to validate its outputs.
The retained function signatures, numerical definitions and ROI behavior are
unchanged. The new header is plain C,
without preprocessor dependencies, platform annotations or C++ linkage blocks.
C++ callers wrap the include in their own `extern "C"` block. The new header uses unsized arrays, carrying forward the earlier Wizard-parser
simplification without changing any intended C ABI type. Fixed C array bounds
would not allocate LabVIEW arrays either: array parameters decay to pointers.
Callers must supply the explicit lengths documented below (ROI 4, center 2,
metrics 4, asymmetry 16, sectors 64).

## Common image and storage contract

Each processing function receives the original row-major, single-channel,
contiguous U16 image: `image_pixels[y*image_width+x]`. There is no row padding,
stride parameter, resizing, U8 conversion or input modification. Effective
8/10/12/14/16-bit samples remain unchanged in U16 storage; normalization is an
internal double-precision affine view of all supplied pixels. Width and height
need not match. Dimensions must be positive and their product <=2147483647.
Int is I32, unsigned short is U16, double is IEEE-754 DBL.

All required pointers must be non-null, allocated, correctly aligned, mutually
disjoint and valid throughout the call. A raw C pointer does not convey its
allocation length, so the DLL cannot detect short arrays. No output memory is
allocated, resized, freed or transferred across the ABI. Per-call workspaces
are private to the DLL. All calls are stateless/reentrant; concurrent callers
must use independent output buffers.

Known overlapping storage returns -1 **before initialization**, leaving outputs
untouched and invalid, including when an output overlaps the source. Otherwise,
non-null writable outputs are initialized before other validation: ROI to zeros,
DBL outputs to NaN, sector diagnostics to [NaN,NaN,0,0].
Invalid addresses/undersized buffers remain caller errors. C++ resource and
processing exceptions are caught at the C boundary and return -4.

## Responsibilities and outputs

### FPF_FindCenteredRoiU16: occasional camera setup/reacquisition

The finder returns geometry only; it does not return cropped image pixels or
configure camera hardware. It uses the proven contrast-component ROI locator as
an initial candidate, constructs a private temporary candidate image, and fits
its robust gradient center. It converts that independently measured subpixel
center to full-frame coordinates, verifies two-ring angular/radial evidence,
and centers the final square using integer offsets.

`roi_out` is I32[4] = **[left,top,width,height]**, zero-based; width=height.
`center_out` is DBL[2] = **[center_x_full,center_y_full]**, independently fitted,
never replaced by the ROI midpoint. Offsets are rounded to nearest integers
so each relative center is normally within 0.5 pixel of `(S-1)/2` (required
engineering tolerance <=0.75 pixel). No clamping to an off-center ROI occurs.

Requested size <0 is invalid. A positive size is exact: if it exceeds the frame
or cannot fit while centered, return -1 with invalid outputs. Requested 0 uses
the largest centered square supported by the frame, starting with
`floor(2*min(cx,cy,width-1-cx,height-1-cy)+1)`. This deliberately retains weak
outer rings instead of choosing the earlier tight contrast bounding box. It is
not a promise of a minimum-bandwidth camera ROI. A usable automatic square must
be at least 24 pixels wide. An exact positive request may be smaller, including
1x1; such geometry is not a promise that its future acquired image is analyzable. A missing region/ring evidence returns -3; failed
center fitting or unusable automatic geometry returns -4. All failures retain
zero ROI and NaN center outputs.

Camera offset/size increments, hardware constraints and verification of the
actually applied ROI belong to LabVIEW. Keep the measured center separate from
any hardware-rounded rectangle.

### FPF_AnalyzeU16: direct numerical measurement

There is **no ROI detection, crop copy or implicit square subset**. The exact
supplied rectangle is normalized and analyzed. Selectors are unchanged:

| ID | Method | Important limits |
|---|---|---|
| 0 | A: opposite-spoke correlation | Independent of full-image gradient fitting; large displacement/ambiguous lags can fail. |
| 1 | B: radial reference | Robust subpixel gradient center; at least three consecutive prominent radial peaks. |
| 2 | C: full 2D Airy fit | Uses B initialization then fits individual 2D pixels; can reject model mismatch/nonconvergence. |
| 3 | D: eight-sector phase/center fit | Uses B initialization then matched sector peaks and robust center/order fit. |

There is no fallback when a selected method fails. `metrics_out` is DBL[4]:

| Index | Value | Units/convention |
|---|---|---|
| 0 | fractional phase | cycles [0,1); bright peaks satisfy q/P+phase=integer |
| 1 | center_x | pixels relative to the supplied image |
| 2 | center_y | pixels relative to the supplied image |
| 3 | effective finesse proxy | squared-radius peak spacing / trough-relative FWHM_q |

`q=(x-cx)^2+(y-cy)^2`. Phase is modulo one free spectral range, not absolute
cavity displacement. For Airy method C the equivalent proxy is
`pi/(2*asin(1/sqrt(F+2)))`; it is not calibrated intrinsic cavity finesse.
Unsupported/clipped outer circles are excluded using
`min(cx,cy,width-1-cx,height-1-cy)` with the existing numerical margins.

The shared rectangular implementation uses width for row stride, separate
width/height bounds and initial center coordinates, and separate x/width,
y/height normalization and Jacobians in the 2D Airy model. Smoothing, spoke
sampling, radial/sector profiles and asymmetry sampling honor both dimensions.
Scale-dependent legacy strides/tolerances use the shorter side. Square wrappers
retain the original operation order, thresholds and iteration counts.

### FPF_AnalyzeAsymmetryU16: direct multi-ring measurement

There is **no ROI detection or cropping**. This reuses the existing 16-sector,
nine-ray-per-sector algorithm, ring identities, trough-relative prominence,
subpixel peak/crossing interpolation, exact q-width conversion, ring
normalization, robust aggregation and independent first-harmonic fits.
All quality thresholds are unchanged; rectangular bounds/stride are generalized.

`asymmetry_out` is DBL[16]:

| Index | Value | Units |
|---|---|---|
| 0 | ring phase | cycles [0,1) |
| 1,2 | center X,Y | supplied-image pixels |
| 3 | finesse proxy | dimensionless |
| 4,5 | brightness Ax,Ay | signed dimensionless |
| 6,7 | linewidth Wx,Wy | signed dimensionless; positive means broader |
| 8 | brightness magnitude | hypot(Ax,Ay) |
| 9 | brightness direction | degrees [0,360) |
| 10 | linewidth magnitude | hypot(Wx,Wy) |
| 11 | linewidth direction | degrees [0,360) |
| 12 | brightness/width correlation | Pearson r [-1,1]; NaN if undefined |
| 13 | valid ring count | integer stored as DBL |
| 14 | valid sector count | integer stored as DBL |
| 15 | combined harmonic RMS | dimensionless residual, not statistical uncertainty |

For s=0..15, `sector_out[4*s+0..3]` contains **normalized prominence, normalized
FWHM_q, matched valid ring count, valid flag**. Invalid sectors are
[NaN,NaN,0,0]. Reshape as 16 rows x 4 columns, keeping fields contiguous.
Angles use image coordinates: 0 right, 90 down, 180 left, 270 up. Width directions
are not inverted. The [existing detailed asymmetry definitions](fringe_asymmetry.md#numerical-procedure-and-quality-limits),
including the precise unweighted combined harmonic RMS formula, remain valid.
Neither coefficients nor directions are mechanical mirror tilt angles; Z,
defocus, illumination and aberrations can affect them. No calibration, feedback
voltages, hardware commands or frame history is implemented.

## Status and validity

| Code | Meaning | Outputs |
|---|---|---|
| 0 | success | Function-specific result valid; asymmetry correlation may be NaN and sectors have flags. |
| -1 | invalid argument or requested ROI geometry | Initialized invalid outputs; overlapping storage is the no-write exception. |
| -2 | reserved future capacity code | Never returned by this API version. |
| -3 | ROI finder could not detect a suitable fringe region | ROI zero, center NaN. Other functions do not return -3. |
| -4 | unreliable numerical result or processing/resource failure | DBL arrays NaN, sectors invalid; ROI zero where applicable. |
| -5 | invalid analysis selector | AnalyzeU16 metrics NaN. |

The analysis functions perform their own numerical validity checks. A separate
pre-screen is unnecessary. On status 0, accept the frame's measurement subject
to application-specific quality checks; on any nonzero status, discard invalid
measurements. Do not turn failed measurements or NaNs into zero phase/center or
arbitrary piezo corrections. LabVIEW decides how to handle consecutive failures
and whether to trigger reacquisition. The DLL contains no regulator logic.
Dimensions below numerical support are still valid image arguments; numerical
analyses return -4.

## LabVIEW acquisition and CLFN setup

During initialization, LabVIEW acquires a full frame and calls the finder. It
adapts returned offsets/sizes to hardware increments, configures the camera and
records the **actual applied** settings. During tracking, acquire directly from
a **fixed camera ROI**, call FPF_AnalyzeU16 and/or FPF_AnalyzeAsymmetryU16 directly,
and check each call's returned status before using its outputs.
Changing the ROI each frame can remove the center displacement a future
controller intends to observe. Recentering is for setup/reacquisition.

All analysis centers are relative to their supplied image. Convert to full-frame
coordinates using actual camera offsets:
`cx_full=actual_camera_left+cx_local`, `cy_full=actual_camera_top+cy_local`.
Never substitute the originally proposed ROI for hardware-applied coordinates.
The finder alone returns full-frame center coordinates.

Use the x86 DLL for **32-bit LabVIEW even on Windows 11 x64**. Use x64 only with
64-bit LabVIEW. Import the matching FPFringe.h through NI's Wizard, or configure
a Call Library Function Node manually if parsing is unsuccessful. Choose exact
FPF_ function name, **C/cdecl**, return I32 by value. The new header has not
been tested in NI's Wizard; native compilation/ABI checks do not establish import
or actual LabVIEW execution.

Common CLFN parameters: image_pixels = 1D U16, Array Data Pointer, input;
width/height = signed I32 by value. Add each function's remaining parameters
in exactly the prototype order:

| Function | Remaining CLFN parameters |
|---|---|
| GetApiVersion | none |
| FindCenteredRoiU16 | requested size I32 value; roi_out 1D I32[4] Array Data Pointer output; center_out 1D DBL[2] Array Data Pointer output |
| AnalyzeU16 | method I32 value; metrics_out 1D DBL[4] Array Data Pointer output |
| AnalyzeAsymmetryU16 | asymmetry_out 1D DBL[16] Array Data Pointer output; sector_out 1D DBL[64] Array Data Pointer output |

GetApiVersion has no image arguments. Arrays are Data Pointers, never Array
Handles. Allocate image width*height U16 elements with verified row-major layout.
Use Initialize Array for each output: I32 zeros of length 4; DBL NaNs of length
2, 4, 16 or 64 as appropriate. Wire these allocated arrays into the input side
of CLFN output terminals so the DLL receives writable storage. Set minimum array
lengths where available and verify Array Size before and after the call. C bounds do not
create or resize LabVIEW arrays, and the DLL cannot repair incorrect allocation.
Keep all buffers disjoint. Route each analysis status through an error case
structure; only status 0 enables measurement use. Asymmetry ring count, sector
count and harmonic RMS remain available as additional quality indicators.

Do not change any existing FPAnalyzer VIs. Test the new four-function wrapper VIs,
Wizard/manual CLFN configuration, actual camera coordinate mapping, persistent
ROI tracking and acquisition timing on the user's Windows/LabVIEW PC.

## Build, tests and packaging

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix package
# Linux; use build/Release/*.exe and *.dll on Windows:
build/fpfringe_benchmark build/fpfringe_benchmark.csv
python examples/fpfringe_ctypes_test.py build/libFPFringe.so build/libFPAnalyzer.so
python examples/fpfringe_validate.py build/libFPFringe.so build/libFPAnalyzer.so data/test_frames
```

Configure Windows separately with MSVC `-A Win32` or `-A x64`, matching Python
bitness. The CI matrix builds/tests both DLLs, checks architecture and exact
exports, confirms FPFringe does not depend on FPAnalyzer.dll, verifies packaged
headers and uploads **FPFringe-windows-x86**, **FPFringe-windows-x64** separately
from unchanged-purpose FPAnalyzer-windows-x86/x64 artifacts. Every new package
contains only its matching FPFringe.dll and FPFringe.h. Test-report artifacts
include fpfringe_benchmark.csv, existing benchmark CSVs and full CTest logs.

The benchmark measures six operations independently: ROI finder, A, B, C, D
and asymmetry, on 320x320, 640x480 and 1280x960 fixtures, with two warm-ups and 21 measured calls.
Median is sorted index 10, p95 nearest-rank index 19. It records build configuration,
pointer architecture, dimensions, successes/failures and end-to-end C-call time.
No additional checking stage is benchmarked or required. No real-time or
optical accuracy guarantee follows from synthetic timing.

Optional validation expects Image0803/0792/0699/0855/0020.bmp. It reports new
full-frame results, explicit finder geometry and old/new comparisons on the same
**externally selected** square samples. Missing images produce exit 77/CTest
skip, not fabricated passing results. It shares the strict grayscale BMP loader
with the earlier validator (8-bit palette or grayscale 24/32-bit uncompressed;
unsupported formats are explicitly rejected). No image files are committed.
See [validation and measured timings](fpfringe_validation.md) for actual evidence.
