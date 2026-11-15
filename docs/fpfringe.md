# FPFringe API 3

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
    unsigned short roi_pixels[],
    int roi_capacity_pixels,
    int roi_out[]);

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

Exports have these exact undecorated names. FPF_GetApiVersion returns **3**;
FP_GetApiVersion in the legacy DLL still returns **2**, independently. API 3
changes the ROI function's binary signature: it now returns original U16 crop
pixels and I32[3] geometry, with no independent center output. Update any API-2
CLFN/generated VI before loading this DLL; the old six-argument call is unsafe.
No checker export is reintroduced. The two analysis signatures and numerical
behavior remain unchanged. The public header is plain C,
without preprocessor dependencies, platform annotations or C++ linkage blocks.
C++ callers wrap the include in their own `extern "C"` block. The new header uses unsized arrays, carrying forward the earlier Wizard-parser
simplification without changing any intended C ABI type. Fixed C array bounds
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
allocated, resized, freed or transferred across the ABI. The ROI extractor
writes only side*side valid U16 elements after a successful capacity check;
unused output tail stays unchanged. Per-call workspaces
are private to the DLL. All calls are stateless/reentrant; concurrent callers
must use independent output buffers.

Known overlapping storage returns -1 **before initialization**, leaving outputs
untouched and invalid, including when an output overlaps the source. Otherwise,
non-null geometry/measurement outputs are initialized before other validation:
ROI to [0,0,0], analysis DBL outputs to NaN and sectors to [NaN,NaN,0,0]. The crop
buffer is never initialized or changed on failure, including insufficient capacity.
Invalid addresses/undersized buffers remain caller errors. C++ resource and
processing exceptions are caught at the C boundary and return -4.

## Responsibilities and outputs

### FPF_FindCenteredRoiU16: localize and extract a centered raw square

Use this operation during camera setup/reacquisition. It returns a U16 square
crop for immediate numerical analysis and its **[left,top,side]** I32[3] geometry
in the full frame. The fitted subpixel center is internal and is not an output.
Camera hardware is never configured by the DLL.

1. **Candidate localization:** normalize the full frame only for internal
   calculations and reuse the established downsampling/local-contrast/connected
   component detector. Its square candidate includes its existing 8% extent
   margin. The final center is not fitted to the large dark full frame.
2. **Local geometric fit:** copy the candidate into a private temporary U16
   region, normalize that region locally and run the unchanged robust gradient
   center estimator. If it fails, retry regions of 1.5 and 2 times the original
   candidate side (ceil, minimum 24, capped by the frame's shorter side), about
   the same candidate midpoint and bounded by the frame. Duplicate capped sizes
   are skipped. At most three fits run; successful fitting stops retries. Convert
   its subpixel center to full-frame coordinates. There is no post-fit ring-count,
   peak-prominence, full-frame evidence or analysis-success gate.
3. **Geometry and copy:** choose a square side, round offsets to the closest
   valid integers and require center displacement from the square midpoint
   <=0.75 pixels on each axis. Validate capacity before writing any pixel, then
   copy source rows without intensity/geometry changes.

Automatic sizing (`requested_roi_size=0`) is deterministic. Let D be the initial
candidate side, (bx,by) its midpoint, and (cx,cy) the fitted full-frame center:

```
d = max(abs(cx-bx), abs(cy-by))
E = D + 2*d
padding_per_side = max(4, 0.05*E)
S0 = max(24, ceil(E + 2*padding_per_side))
M = floor(2*min(cx,cy,width-1-cx,height-1-cy) + 1)
side = min(S0, M, width, height)
```

This retains the detected extent, adds a modest trough/outer-ring margin and
accounts for the difference between bounding-box and fitted center. It does not
select the largest possible frame square unless that is the needed boundary
limit. Automatic side <24 returns -4. Initial candidate overestimation or weak
outer rings below its contrast threshold remain limitations; no real camera
accuracy guarantee follows from the sizing heuristic.

For positive requested size, use that side **exactly**, even if smaller than a
useful analysis image. Negative size is -1. Round `(cx-(side-1)/2,cy-(side-1)/2)`
and constrain offsets to the frame; if no valid offsets satisfy the 0.75-pixel
centering tolerance, return -1. No padding, interpolation, resizing or silent
side adjustment is applied. The center is fitted geometry, never a brightest
pixel or an assigned ROI midpoint.

`roi_pixels` is caller-owned U16 storage, flat row-major **side x side**. The
capacity is **U16 elements, not bytes**, and must be nonnegative. All returned
samples are bit-for-bit copies:
`roi_pixels[y*side+x]=image_pixels[(top+y)*image_width+left+x]`.
Source pixels remain unchanged. On success exactly side*side values are valid;
the tail remains untouched. No DLL allocator may resize a LabVIEW array pointer.

If capacity is too small, return **-2**, populate the valid three-element geometry
and write **no pixels**. Retry with at least side*side U16 elements. Every pointer
must remain non-null, including a zero-capacity query: provide a disjoint
allocated one-element U16 placeholder. Known overlaps return -1 before all
writes. Other errors (-1/-3/-4) leave crop storage unchanged and geometry zero.
A missing candidate is -3, failed local center fitting/resource failure is -4.

For one call, allocate min(width,height)^2 U16 elements (dimensions/product must
fit I32). Alternatively query capacity 0, allocate the returned side^2 and retry
on the **same source frame**; querying/retrying repeats localization. Pass the
successful crop directly to either analysis function with width=height=side.
Camera ROI increments, actual applied offsets and hardware settings remain
LabVIEW responsibilities; a proposed ROI is not confirmation of camera settings.

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
| -2 | ROI crop capacity insufficient | Valid [left,top,side], crop untouched; allocate side*side U16 and retry. |
| -3 | ROI finder could not detect a suitable fringe region | ROI zero, crop untouched. Other functions do not return -3. |
| -4 | unreliable numerical result or processing/resource failure | Analysis DBL arrays NaN, sectors invalid; finder ROI zero and crop untouched. |
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

During initialization, LabVIEW acquires a full frame and calls the extractor.
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
The extractor returns full-frame ROI coordinates only; center outputs from the
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
| FindCenteredRoiU16 | requested size I32 value; roi_pixels 1D U16 Array Data Pointer output; capacity I32 value (elements); roi_out 1D I32[3] Array Data Pointer output |
| AnalyzeU16 | method I32 value; metrics_out 1D DBL[4] Array Data Pointer output |
| AnalyzeAsymmetryU16 | asymmetry_out 1D DBL[16] Array Data Pointer output; sector_out 1D DBL[64] Array Data Pointer output |

For the ROI extractor, configure CLFN parameters in this exact order:

| Parameter | LabVIEW configuration |
|---|---|
| image_pixels | 1D U16 Array Data Pointer, input |
| image_width | I32, value |
| image_height | I32, value |
| requested_roi_size | I32, value |
| roi_pixels | 1D U16 Array Data Pointer, caller-allocated output |
| roi_capacity_pixels | I32, value; count of U16 elements |
| roi_out | 1D I32 Array Data Pointer, 3 elements, output |
| Return | I32, value |

Set roi_out minimum length to 3. Wire allocated output arrays into the CLFN input
terminals; the DLL does not allocate them. On status 0, trim the U16 array to
side*side and reshape row-major to side x side. On -2, geometry is usable for
allocation only; there are no valid crop pixels yet. On other errors, discard
outputs. Remove the old center-output terminal rather than leaving an unused
extra parameter. Check GetApiVersion==3 and use matching DLL/header/VI versions.

GetApiVersion has no image arguments. Arrays are Data Pointers, never Array
Handles. Allocate image width*height U16 elements with verified row-major layout.
Use Initialize Array for the extractor's outputs: U16 storage with capacity
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

The benchmark measures six operations independently: ROI finder, A, B, C, D
and asymmetry, on 320x320, 640x480 and 1280x960 fixtures, with two warm-ups and 21 measured calls.
Median is sorted index 10, p95 nearest-rank index 19. It records build configuration,
pointer architecture, dimensions, successes/failures and end-to-end C-call time.
No additional checking stage is benchmarked or required. No real-time or
optical accuracy guarantee follows from synthetic timing.

Optional validation prioritizes Image0180.bmp and also accepts the earlier
Image0803/0792/0699/0855/0020.bmp inputs. It reports new
full-frame results, extractor geometry, a source-pixel equality check and old/new
analyses on the same **DLL-returned raw square crop**. Missing images produce exit 77/CTest
skip, not fabricated passing results. It shares the strict grayscale BMP loader
with the earlier validator (8-bit palette or grayscale 24/32-bit uncompressed;
unsupported formats are explicitly rejected). No image files are committed.
See [validation and measured timings](fpfringe_validation.md) for actual evidence.
