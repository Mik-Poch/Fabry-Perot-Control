# FPAnalyzer 0.1

Single-frame Fabry–Pérot fringe analysis for a LabVIEW instrument application.
The production targets are **Windows x86 and x64, MSVC 2022, C++17**. The primary
integration is **32-bit LabVIEW on Windows 11 64-bit**, using the x86 DLL.
The DLL uses the C++
standard library only: no Python, OpenCV, GPU, LabVIEW runtime, or external
image-processing dependency. Python is used only for an independent development
test. No actuator control, frame history, absolute calibration or optimization
is implemented.

## Download the Windows DLL

On this repository's **Actions** tab, select **Windows x86 and x64 DLL**, open a successful
run for the desired commit, and download **FPAnalyzer-windows-x86** for 32-bit
LabVIEW or **FPAnalyzer-windows-x64** for 64-bit LabVIEW under
**Artifacts** (GitHub sign-in may be required). Extract `bin/FPAnalyzer.dll` and
`include/FPAnalyzer.h`. The artifact is uploaded only after the MSVC build,
native tests, C caller, Python ctypes test and exact export-name check succeed.
Test output is available in the job log and the matching
**FPAnalyzer-test-report-x86** or **FPAnalyzer-test-report-x64** artifact.
An artifact from another commit is not validation of your current checkout.

Match the DLL to **LabVIEW's bitness**, not Windows' bitness. Install the Microsoft
Visual C++ 2015–2022 Redistributable (x86 for the x86 DLL, x64 for the x64 DLL)
if needed: Release builds use the normal dynamic MSVC runtime. See
[LabVIEW integration](docs/labview_integration.md) for the Wizard and manual node
configuration. Actual LabVIEW execution has **not** been verified.

## Build with Visual Studio 2022

Install Visual Studio 2022 (or Build Tools) with **Desktop development with C++**,
the MSVC x86/x64 toolset, Windows SDK, and CMake >=3.20. Python 3 of the **same
architecture as the DLL** is optional for local ctypes tests and required in CI.
From the repository root, using x86 Python for Win32:

```powershell
$python = (Get-Command python).Source # select x86 Python on PATH first
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32 "-DPython3_EXECUTABLE=$python"
cmake --build build-x86 --config Release --parallel 4
ctest --test-dir build-x86 -C Release --output-on-failure --verbose
cmake --install build-x86 --config Release --prefix package-x86
python examples/python_ctypes_test.py build-x86/Release/FPAnalyzer.dll
```

For x64, select x64 Python, replace `-A Win32` with `-A x64` and use separate
`build-x64` / `package-x64` directories. Do not reuse a CMake build directory
when changing architectures. CI builds and tests both with matching Python.

The x86 DLL is `build-x86/Release/FPAnalyzer.dll`; distribution files are in
`package-x86/`. `dumpbin /exports build-x86/Release/FPAnalyzer.dll` must show only
`FP_GetApiVersion` and `FP_AnalyzeFrameU8`, undecorated. The public header compiles
as C99; the C ABI test links and calls the actual shared library.

Linux is a development/testing platform, not a way to generate the Windows DLL:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --verbose
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DFP_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel 4
ctest --test-dir build-asan --output-on-failure
```

The Linux output is `build/libFPAnalyzer.so`. Sanitizer builds test native code
and the C caller; ctypes is tested in the normal build to avoid loading a
sanitized library into an unsanitized Python process.

## API and data layout

```c
int FP_GetApiVersion(void); /* returns 1 */
int FP_AnalyzeFrameU8(const unsigned char image_pixels[],
    int image_width, int image_height, unsigned char cropped_pixels[],
    int cropped_capacity_pixels, int roi_out[3], double metrics_out[4]);
```

Both use C linkage and `cdecl`. All buffers belong to the caller. Input is U8
grayscale, row-major, no row padding: `image_pixels[y*width+x]`. Coordinates are
zero-based, X rightward, Y downward; integer coordinates refer to pixel centers.
The API requires 32-bit `int` and 64-bit IEEE-754 `double`. The image product must
fit signed I32 (at most `INT_MAX` pixels). Width and height must be positive;
images smaller than 24 pixels on either side cannot provide an ROI.

| Output | Meaning |
| --- | --- |
| `roi_out[0..2]` | `[left_x, top_y, square_size]` in the source image |
| `cropped_pixels[0..size*size-1]` | Exact square crop, row-major, no padding |
| `metrics_out[0]` | Fractional phase in [0,1), convention below |
| `metrics_out[1..2]` | Subpixel ring-center X,Y **relative to the crop** |
| `metrics_out[3]` | Positive dimensionless image-derived finesse proxy |

All pointers must be non-null and storage must be disjoint. Input allocation
must contain at least `width*height` elements, crop allocation at least the
declared capacity, ROI allocation at least 3 I32s and metrics at least 4 DBLs.
The function cannot verify actual allocation lengths or pointer validity. Do
not pass LabVIEW handles, strided images, packed RGB data or overlapping arrays.

| Status | ROI | Crop | Metrics |
| --- | --- | --- | --- |
| `0` success | valid | exactly `size*size` valid bytes | all four valid |
| `-1` invalid argument | zero | untouched | NaN |
| `-2` insufficient capacity | valid; required capacity = `size*size` | untouched | NaN |
| `-3` no detectable fringe region | zero | untouched | NaN |
| `-4` insufficient reliable measurement | valid if size > 0 | valid if size > 0 | NaN |

On entry the function initializes non-null ROI and metrics buffers even if
another pointer is null. **Exception:** detected overlapping storage is rejected
before initialization; outputs are untouched and invalid in that case. A
resource/allocation failure is contained at the ABI and mapped to `-4`; size
zero then means no crop was produced. No C++ exception crosses the API.
The crop buffer tail is always untouched. Capacity is checked before copying.
For simple LabVIEW integration allocate `min(width,height)^2` crop elements (or
`width*height`); a smaller capacity may return `-2` with retry geometry. Call again
on the same frame after resizing. Calls keep no shared mutable state.

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

This v0.1 method assumes a reasonably circular, well-resolved, dominant ring
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

`ctest` runs the native suite, pure C caller and independent Python ctypes
loader. Optional [real-frame tests](data/test_frames/README.md) report **Skipped**
when no `.pgm` frames are supplied; they are not counted as accuracy validation.

See [validation record](docs/validation.md) for measured timing and which
platforms have actually been exercised. GitHub Actions validates Windows on
each push/PR; a workflow file alone does not prove that run has passed.
