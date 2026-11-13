# FPAnalyzer v3 — four analysis methods, compatible ABI v2

Single-frame Fabry–Pérot fringe analysis for **Windows Win32/x86 (primary,
32-bit LabVIEW)** and x64, MSVC 2022, C++17. The DLL uses only the C++ standard
library. No Python, OpenCV, GPU or LabVIEW runtime is required by the DLL.
No actuator control, frame history, absolute calibration or A/B/C/D methods.

## Compatibility and method selection

This feature release preserves **FP_AnalyzeFrameU16 exactly**: same signature,
method B, ROI handling, output lengths and status meanings. **FP_GetApiVersion
still returns 2**, so existing `version == 2` checks and working LabVIEW callers
continue to work. Version is the ABI compatibility generation; v3 names this
feature release. To discover the additive selector, resolve the new export
`FP_AnalyzeFrameU16WithMethod` (`GetProcAddress`/ctypes attribute), or use an
artifact from this release. Version 2 alone cannot distinguish older builds.

| ID | Method | Implementation / role |
| --- | --- | --- |
| 0 | A — opposite-spoke correlation | Eight sampled spokes, four signed opposite-pair lags, subpixel refinement; sparse radial profile for all four metrics. Fast candidate, no full-image gradient center fit. |
| 1 | B — circle center and radial profile | Existing robust center, squared-radius profile, peaks, phase and FWHM; optimized buffers and repeated fit terms. Original function always uses this method. |
| 2 | C — 2D Airy fit | Radial initialization, bounded robust Levenberg–Marquardt over sampled 2D pixels, background plane and illumination slopes, convergence/quality rejection. |
| 3 | D — angular sectors | Radial initialization, eight independent sector profiles; robust joint peak-order/center/phase fit with periodicity handled by integer orders. At least five valid sectors. |

All paths use the same crop and phase convention; a selected algorithm's failure
returns -4 with NaNs, **never another method's results**. Invalid method returns
-5. C/D use B only as an initializer. No method is assumed best on every image.
See [algorithms, accuracy and measured performance](docs/methods_and_performance.md)
for equations, limits, before/after B comparisons and complete four-metric timing.
No camera images were supplied; synthetic results are not real-camera accuracy.

U8 was removed in ABI v2 and remains absent. The permanent public files are
**FPAnalyzer.dll** and **FPAnalyzer.h**, a single simplified plain C header.

## DLL versus the LabVIEW wrapper

The user-facing **FP Analyze Frame U16.vi** has exactly four inputs:
`image_pixels` (1D U16), `image_width` (I32), `image_height` (I32), and
`roi_in` (1D I32, three elements; empty is converted to `[0,0,0]` internally).
It returns `cropped_pixels` (1D U16), `roi_out` (exactly 3 I32),
`metrics_out` (exactly 4 DBL), and `status` (I32).

The original wrapper remains unchanged. A selector wrapper named
**FP Analyze Frame U16 With Method.vi** adds only `analysis_method` (I32) as a
fifth input; it has the same four outputs. The DLL uses caller-owned buffers:

```c
int FP_GetApiVersion(void);
int FP_AnalyzeFrameU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    const int roi_in[3],
    unsigned short cropped_pixels[],
    int cropped_capacity_pixels,
    int roi_out[3],
    double metrics_out[4]);
int FP_AnalyzeFrameU16WithMethod(
    const unsigned short image_pixels[], int image_width, int image_height,
    const int roi_in[3], int analysis_method,
    unsigned short cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[3], double metrics_out[4]);
```

The wrapper allocates buffers and computes capacity internally; capacity is
**not** a connector-pane input. For width W and height H use `min(W,H)^2` U16
elements, checking dimensions, image length, I64 size arithmetic, I32 limits
and allocation failure first. Initialize result arrays to exactly 3 and 4
items; after the call trim the crop to `roi_out[2]^2` valid pixels only when
status is 0, or -4 with a positive ROI size. Never resize a LabVIEW Array Data
Pointer in the DLL with `malloc`, `realloc` or `new`.

See [precise LabVIEW wiring and generated-VI corrections](docs/labview_integration.md).
There is no generated/verified `.vi` in this repository: actual LabVIEW wiring
and execution require manual verification. The earlier simplified header was
successfully imported according to the user; that does not verify the v2 node.

## ROI and intensity handling

- `[0,0,0]`: automatically detect the fringe region, choose a square and crop.
- `[left_x,top_y,square_size]` with positive size: validate and use exactly that
  region. No expansion, relocation, full-frame detection, full-frame downsampling
  or full-frame statistics occur. Invalid/out-of-bounds ROIs return -1.
- Both paths normalize the **crop** using its 1st/99th U16 percentiles mapped to
  200 double-precision analysis units (full-range fallback). The auto detector
  additionally uses full-frame normalization for detection only. The histogram
  avoids full-frame floating-point buffers; no U8 conversion occurs. Original
  pixels, including 0 and 65535, are returned bit-for-bit unchanged.
- An allocated three-element `roi_in` is mandatory at the C boundary. Raw
  pointers carry no array length; the wrapper must check/normalize input first.

All buffers must be non-null and mutually disjoint, including `roi_in` and
`roi_out`. Input is contiguous row-major U16, no padding:
`image_pixels[y*width+x]`. Positive dimensions and their product must fit signed
I32; buffers may require much more memory than available in a 32-bit process.
Coordinates are zero-based, X right, Y down, integer locations at pixel centers.
A small but valid manual ROI is copied exactly and can return -4 if too small
for numerical analysis (center estimation requires at least 24×24).

| Output | Definition |
| --- | --- |
| `roi_out[3]` | `[left_x,top_y,square_size]` in the source frame |
| `cropped_pixels` | Exactly `square_size^2` valid original U16 elements |
| `metrics_out[4]` | `[ring_phase,center_x,center_y,finesse_proxy]` |
| center | Subpixel coordinates relative to the cropped image |
| ring phase | Fractional phase in [0,1), convention below |
| finesse proxy | Positive, dimensionless, image-derived spacing/linewidth |

| Status | ROI | Crop | Metrics |
| --- | --- | --- | --- |
| 0 success | valid | valid | all four valid |
| -1 invalid arguments/ROI | zero | untouched | four NaNs |
| -2 insufficient capacity | valid, required capacity = size² | untouched | four NaNs |
| -3 no fringe region (auto only) | zero | untouched | four NaNs |
| -4 insufficient measurement/resources | valid if size > 0 | valid if size > 0 | four NaNs |
| -5 invalid method (selector only) | zero | untouched | four NaNs |

Non-null ROI/metrics buffers are initialized even when another argument is
invalid. Detected overlap is rejected before any writes, leaving outputs
untouched and invalid. All actual allocation lengths are caller obligations.
The DLL writes exactly 3 ROI and 4 metric elements and never touches the crop
tail. Exceptions are contained as -4. Calls keep no shared mutable state.

## Build and download

GitHub Actions **Windows x86 and x64 DLL** builds both architectures with
matching Python for ctypes, tests the compiled DLL, verifies exact exports and
packages only `bin/FPAnalyzer.dll` and `include/FPAnalyzer.h`. Download
**FPAnalyzer-windows-x86** for 32-bit LabVIEW or **FPAnalyzer-windows-x64** for
64-bit LabVIEW from the successful run for the desired commit. Test reports
include per-method accuracy rows and methods_benchmark.csv with native/stage
median, p95, min/max and success/failure counts for each architecture. Match LabVIEW's bitness, not Windows'.
Install the matching Microsoft Visual C++ 2015–2022 Redistributable if needed.

Install Visual Studio 2022 / Build Tools with Desktop development with C++,
Windows SDK and CMake >=3.20. From the repository root (select x86 Python):

```powershell
$python = (Get-Command python).Source
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32 "-DPython3_EXECUTABLE=$python"
cmake --build build-x86 --config Release --parallel 4
ctest --test-dir build-x86 -C Release --output-on-failure --verbose
cmake --install build-x86 --config Release --prefix package-x86
cmake -DPROJECT_DIR=. -DPACKAGE_DIR=package-x86 -P tests/check_public_header.cmake
```

For x64 select x64 Python and use `-A x64` with separate build/package folders.
`dumpbin /exports` must show exactly `FP_GetApiVersion` and
`FP_AnalyzeFrameU16` and `FP_AnalyzeFrameU16WithMethod`, undecorated. C callers include `FPAnalyzer.h` directly;
C++ callers wrap its include in `extern "C" { ... }` in their own source.
The public header has no compiler macros or platform conditionals. MSVC uses
cdecl (`/Gd`); Python uses `ctypes.CDLL`.

Linux is for development/algorithm tests, not producing the Windows DLL:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --verbose
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DFP_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel 4
ctest --test-dir build-asan --output-on-failure
./build/fp_roi_benchmark
./build/fp_methods_benchmark 21 build/methods_benchmark.csv
# Optional, same-architecture previous library with the original U16 signature:
./build/fp_b_compare /absolute/path/to/previous/libFPAnalyzer.so
```

The Linux library is `build/libFPAnalyzer.so`. Sanitizers exercise native code;
ctypes runs in the regular build to avoid mixing a sanitized library with an
unsanitized Python host. Python tests use only the standard library. Optional
real-camera PGM frames are skipped when absent, not counted as validation.

## Algorithm and scientific conventions

Internal stages are separate in `src/Analysis.h` / `src/Analysis.cpp`:

1. Box-downsample to at most about 400 pixels on the long axis, smooth, measure
   local standard deviation in 8×8 tiles, threshold contrast, dilate the tile
   mask to join nearby rings, and select a supported connected region. Scores
   are capped to reduce hot-pixel influence. Expand its bounding square by 8%
   and clamp it to the image. Neither ROI nor center uses the brightest pixel.
2. Copy the original, unsmoothed pixels. Fit the intersection of intensity
   gradient normals using weighted least squares and robust reweighting. For a
   circular fringe, `(gy,-gx) dot (center-pixel) = 0`. Reject degenerate fits and
   poorly supported centers; use floating-point center coordinates.
3. Average raw intensities in **squared-radius** bins,
   `q = (x-cx)^2 + (y-cy)^2`, using linear bin splatting and light smoothing.
   Only complete circles inside the crop contribute. At most about 4096 bins
   limit cost. No missing outer arc is extrapolated.
4. Find prominent peaks with parabolic sub-bin interpolation. Require at least
   three consecutive peaks with approximately constant squared-radius spacing,
   at least 12 bins per spacing, and small residuals in a linear order fit.
   Inadequate sampling, contrast, center support or spacing yields `-4`.
5. Fit `q_n = intercept + spacing*n`. The phase convention is
   **`ring_phase = frac(-intercept/spacing)`**, where `frac(t)=t-floor(t)`.
   Bright peaks satisfy `q/spacing + ring_phase = integer`. Thus the synthetic
   Airy model `background + amplitude/(1 + A*sin²(pi*(q/spacing + phase)))`
   returns `phase mod 1`, independent of which integer ring is first observed.
   This convention increases phase by +δ for a +δ change in that model. A sign
   or wavelength relation to actuator displacement needs separate calibration.
6. For each resolved peak, baseline is the **mean of the neighboring trough
   minima**, one on each side. FWHM is the distance in q between interpolated
   crossings of `baseline + (peak-baseline)/2`. Require at least two resolved
   widths; report `spacing / median(FWHM_q)`.

Phase is a cavity-phase observable modulo one FSR, **not absolute mirror
separation**. The finesse proxy includes camera PSF, sampling, illumination,
alignment and noise; it is not claimed to be intrinsic optical finesse.

The reference radial method assumes a reasonably circular, well-resolved, dominant ring
pattern on a relatively dark background. The ROI may change by tiles between
frames; crop-relative centers must be translated using ROI offsets for camera
coordinates. Strong aberration/ellipticity, competing patterns, obscured centers,
severe clipping, few rings, saturation and dense unresolved fringes may fail or
bias measurements. Thresholds are conservative engineering heuristics, not a
calibrated confidence estimate. No real-camera accuracy claim is made yet.

## Validation

Tests synthesize finite-aperture Airy-like images with known fractional center,
phase, squared-radius spacing, linewidth, brightness, noise and translation.
They verify crop contents and bounds, exact/insufficient capacity, canaries,
null/invalid inputs and overflow dimensions, constant/hot-pixel/noise/stripe
rejection, insufficient rings, portrait/square/landscape dimensions, modulo-one
phase and finesse response to broader rings. Core tests also check an analytic
profile's FWHM against its closed-form value. Checks remain active in Release.

`ctest` runs original and all-method native suites, concurrency/regression tests,
the Release method/stage benchmark, pure C caller and independent Python ctypes
loader. Optional [real-frame tests](data/test_frames/README.md) report **Skipped**
when no `.pgm` frames are supplied; they are not counted as accuracy validation.

See [validation record](docs/validation.md) for measured timing and which
platforms have actually been exercised. GitHub Actions validates Windows on
each push/PR; a workflow file alone does not prove that run has passed.
