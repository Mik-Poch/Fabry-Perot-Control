# FPFringe API 4

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
    unsigned short cropped_pixels[],
    int cropped_capacity_pixels,
    int roi_out[3]);

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

Exports have these exact undecorated names. FPF_GetApiVersion returns **4**;
FP_GetApiVersion in the legacy DLL remains **2**, independently. API 4 changes
only the ROI entry point's signature/implementation and version. It takes six
arguments and delegates the exact legacy automatic-ROI/method-0 pipeline.
Replace older generated VI/CLFN configurations before calling it. There is no
requested-size control or separate center output. The two direct numerical
analysis signatures and calculations remain unchanged. The public header is
plain C,
without preprocessor dependencies, platform annotations or C++ linkage blocks.
C++ callers wrap the include in their own `extern "C"` block. The image/crop/analysis arrays remain ordinary C array parameters; roi_out[3]
explicitly documents the three-I32 geometry allocation. Fixed C array bounds
would not allocate LabVIEW arrays either: array parameters decay to pointers.
Callers must supply explicit lengths: ROI geometry 3 I32, crop capacity in U16
elements, metrics 4 DBL, asymmetry 16 DBL and sectors 64 DBL.

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
allocated, resized, freed or transferred across the ABI. The ROI pipeline
checks crop capacity before copying side*side original U16 pixels; its unused
output tail stays unchanged. A numerical method-0 failure can follow a complete
crop copy, exactly as in legacy. Per-call workspaces
are private to the DLL. All calls are stateless/reentrant; concurrent callers
must use independent output buffers.

The ROI wrapper inherits **all** validation, overlap, initialization and exception
behavior from fp::analyzeFrame, without an extra validation layer. Known overlap
returns -1 before writes; otherwise non-null roi_out is initialized to zeros.
For direct numerical calls, DBL outputs are initialized to NaN and invalid sectors
to [NaN,NaN,0,0]. Invalid addresses/short allocations cannot be detected from raw
pointers. C++ exceptions are contained by the existing implementation.

## Responsibilities and outputs

### FPF_FindCenteredRoiU16: exact legacy automatic ROI and method 0

This is a thin wrapper around **fp::analyzeFrame**, linked from the shared static
fp_core, using `roi_in={0,0,0}`, method `0` and a private unused DBL[4] metrics
array. Source, dimensions, caller crop pointer, capacity and ROI pointer pass
directly to that function. It does not call or load FPAnalyzer.dll at runtime.

The full proven legacy pipeline runs: automatic region detection, original-U16
square cropping, crop normalization, method-0 opposite-spoke analysis and its
existing acceptance checks. Its metrics are discarded, not returned. The ROI,
crop bytes and I32 status are exactly those of FP_AnalyzeFrameU16WithMethod with
automatic ROI and method 0 on identical data. No candidate expansion, local
refitting, separate sizing formula, re-centering rule or extra gate remains.
Pipeline.cpp, Methods.cpp and Analysis.cpp are unchanged.

The historical function name does **not** promise an independently re-centered
square or a 0.75-pixel midpoint bound. It exposes the legacy detected ROI as-is.
There is no exact requested-size input and no independent center output.
`roi_out[3]` is **[left,top,square_size]** in zero-based full-frame coordinates.
`cropped_pixels` contains the original samples in flat row-major side x side
order. The source remains unchanged; no U8 conversion/resampling is performed.

Capacity counts **allocated U16 elements, not bytes**. On -2 the ROI is valid
and no crop pixels are written. On -4, method 0 may have failed **after** copying:
if ROI side is positive, geometry and all side*side diagnostic pixels are valid.
Do not convert that failure into status 0. A resource failure before copy can
instead leave ROI zero. These distinctions are intentionally identical to legacy.
On -1/-3 the crop is untouched and ROI zero, except known overlap rejects before
all initialization. Crop tails are untouched.

**A capacity value does not allocate memory.** For 1280x960, the maximum square
crop is 960x960 = **921600 U16 elements = 1843200 bytes**. LabVIEW must allocate
that actual array before calling the DLL. Passing capacity 921600 with an empty,
8194-element or incorrectly typed array permits out-of-bounds writes and can
crash the caller; a raw pointer cannot reveal the actual allocation length.
Wrong argument order or using the earlier ABI is another possible crash source.
Without the reported VI/crash dump, the exact cause on the user's machine is
not independently confirmed. The native regression uses a real 921600-element
allocation with guard values, not just that capacity integer.

A caller may instead query with a non-null crop placeholder and capacity 0, then
on -2 allocate side*side and retry on the same frame; this is the existing legacy
capacity convention. It does not avoid re-running detection. No DLL function may
allocate, resize or free LabVIEW-owned array storage.

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

| Code | ROI wrapper (exact legacy method 0) | Direct numerical analyses |
|---|---|---|
| 0 | ROI, crop and method-0 execution successful; exactly side*side raw pixels valid. | Measurements valid; undefined correlation may be NaN and sectors have flags. |
| -1 | Invalid argument/overlap; ROI zero and crop untouched, except overlap leaves outputs untouched. | Initialized invalid outputs, except overlap rejects before writes. |
| -2 | Crop capacity insufficient: valid geometry, no pixel writes. | Not returned. |
| -3 | Automatic legacy ROI detection failed: ROI zero, crop untouched. | Not returned. |
| -4 | Method-0/resource failure; positive ROI side indicates a complete diagnostic crop, otherwise no crop produced. | DBL metrics NaN and sectors invalid. |
| -5 | Not returned because the wrapper always selects method 0. | Invalid analysis selector; metrics NaN. |

Accept a measurement only on status 0, subject to application quality checks.
Do not use failed/NaN results for arbitrary piezo corrections. LabVIEW handles
consecutive failures or reacquisition; the DLL implements no regulator logic.

## LabVIEW acquisition and CLFN setup

During initialization, LabVIEW acquires a full frame and calls the ROI wrapper.
It can immediately analyze the returned U16 square, or adapt the returned
coordinates to camera increments, configure hardware and record the **actual
applied** settings. During tracking, acquire directly from
a **fixed camera ROI**, call FPF_AnalyzeU16 and/or FPF_AnalyzeAsymmetryU16 directly,
and check each call's returned status before using its outputs.
Changing the ROI each frame can remove the center displacement a future
controller intends to observe. Recentering is for setup/reacquisition.

All analysis centers are relative to their supplied image. Convert to full-frame
coordinates using actual camera offsets:
`cx_full=actual_camera_left+cx_local`, `cy_full=actual_camera_top+cy_local`.
Never substitute the originally proposed ROI for hardware-applied coordinates.
The ROI wrapper returns full-frame ROI coordinates only; center outputs from the
analysis functions are relative to their own input image or returned crop.

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
| FindCenteredRoiU16 | cropped_pixels 1D U16 Array Data Pointer output; cropped_capacity_pixels I32 value (elements); roi_out 1D I32[3] Array Data Pointer output |
| AnalyzeU16 | method I32 value; metrics_out 1D DBL[4] Array Data Pointer output |
| AnalyzeAsymmetryU16 | asymmetry_out 1D DBL[16] Array Data Pointer output; sector_out 1D DBL[64] Array Data Pointer output |

For the ROI wrapper, configure CLFN parameters in this exact order:

| Parameter | LabVIEW configuration |
|---|---|
| image_pixels | 1D U16 Array Data Pointer, input |
| image_width | I32, value |
| image_height | I32, value |
| cropped_pixels | 1D U16 Array Data Pointer, caller-allocated output |
| cropped_capacity_pixels | I32, value; count of U16 elements |
| roi_out | 1D I32 Array Data Pointer, 3 elements, output |
| Return | I32, value |

Configure exactly **six** parameters. Delete the obsolete requested-size and
center-output terminals. Select **C/cdecl**, I32 return by value and **Array Data
Pointer**, never Array Handle. Check GetApiVersion==4 before the ROI call.

For a 1280x960 frame, the wrapper VI allocation/wiring is:

1. Use Initialize Array with a **U16 zero constant**, dimension size **921600**.
   This creates actual storage. Wire this array to the crop parameter's input
   terminal; retain/read the modified array on its output terminal.
2. Wire **Array Size of that actual allocated array** as I32
   cropped_capacity_pixels. The integer must not exceed the allocation.
3. Initialize a separate **I32 array of exactly 3 elements** for roi_out, wire it
   into the corresponding CLFN parameter and set minimum size 3 where supported.
4. Verify input Array Size equals width*height, output allocation >=capacity,
   U16 element type and matching DLL bitness before the call. Never pass empty
   output arrays and rely on numeric capacity/minimum-size annotations to grow them.
5. On status 0, trim to side*side and reshape row-major. On -2, use geometry to
   allocate and retry without reading crop pixels. On -4 with positive side,
   a complete crop can be inspected diagnostically; numerical operation failed.
   Guard controller use with each numerical call's status.

For arbitrary dimensions, calculate min(width,height)^2 using a wider integer,
check it fits I32 and allocate that many U16 elements. No C allocator or LabVIEW
Memory Manager calls are used by the DLL on these pointers.

GetApiVersion has no image arguments. Arrays are Data Pointers, never Array
Handles. Allocate image width*height U16 elements with verified row-major layout.
Use Initialize Array for the ROI wrapper's outputs: U16 storage with capacity
min(width,height)^2 (or the query/retry flow above) and I32 zeros of length **3**.
For numerical analyses allocate DBL NaNs of length 4, 16 or 64 as appropriate. Wire these allocated arrays into the input side
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

The benchmark measures six operations independently: legacy automatic ROI plus method 0, A, B, C, D
and asymmetry, on 320x320, 640x480 and 1280x960 fixtures, with two warm-ups and 21 measured calls.
Median is sorted index 10, p95 nearest-rank index 19. It records build configuration,
pointer architecture, dimensions, successes/failures and end-to-end C-call time.
No additional checking stage is benchmarked or required. No real-time or
optical accuracy guarantee follows from synthetic timing.

ROI timing includes the complete legacy method-0 analysis whose metrics are discarded;
it is not a crop-only measurement.

Optional validation prioritizes Image0180.bmp and also accepts the earlier
Image0803/0792/0699/0855/0020.bmp inputs. It reports new
full-frame results, exact legacy/new ROI statuses, geometry and complete crop buffers,
a source-pixel equality check and old/new
analyses on the same **DLL-returned raw square crop**. Missing images produce exit 77/CTest
skip, not fabricated passing results. It shares the strict grayscale BMP loader
with the earlier validator (8-bit palette or grayscale 24/32-bit uncompressed;
unsupported formats are explicitly rejected). No image files are committed.
See [validation and measured timings](fpfringe_validation.md) for actual evidence.
