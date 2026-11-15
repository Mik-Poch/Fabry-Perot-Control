# U16 fringe asymmetry (additive ABI v2 feature)

Discover `FP_AnalyzeFringeAsymmetryU16` with GetProcAddress/dlsym (or resolve it
with ctypes). `FP_GetApiVersion()` remains **2**. The existing two analysis
functions, their numerical implementations, signatures and status codes are
unchanged. The new export has C linkage, cdecl and no decorated name.

## Calling and output validity

```c
int FP_AnalyzeFringeAsymmetryU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    const int roi_in[],
    int roi_out[],
    double asymmetry_out[],
    double sector_out[]);
```

The new public declaration deliberately uses unsized arrays to simplify parsing
by NI's Import Shared Library Wizard. In C function parameters, `T a[]`,
`T a[N]` and `T *a` describe the same pointer ABI; removing the bounds does not
change the seven arguments, C linkage, cdecl, exported name or API version.
It does not alter the implementation or numerical/error behavior. Plain C/C++
compilation and native ABI calls validate those properties; they do **not**
validate NI's parser. The final Wizard import must be checked in actual LabVIEW.
The three previously working public declarations remain unchanged.

Unsized arrays are still pointers to caller-owned memory, with these required
allocations (the DLL cannot infer their lengths):

| Parameter | LabVIEW type | Required length |
|---|---|---|
| image_pixels | 1D U16, Array Data Pointer | image_width * image_height |
| image_width | I32, value | -- |
| image_height | I32, value | -- |
| roi_in | 1D I32, Array Data Pointer, input | 3 |
| roi_out | 1D I32, Array Data Pointer, output | 3 |
| asymmetry_out | 1D DBL, Array Data Pointer, output | 16 |
| sector_out | 1D DBL, Array Data Pointer, output | 64 |
| Return | I32, value | -- |

Allocate all arrays before calling; storage must be mutually disjoint and remain
alive during the call. Input is row-major native U16; no U8 conversion occurs.
Positive dimensions and their product must fit I32. An invalid pointer or a short
allocation cannot be made safe by this interface; allocation lengths are caller
obligations. The DLL never allocates or resizes LabVIEW Array Data Pointers
and transfers no memory ownership. Its internal crop allocation is separate.
Automatic ROI is `[0,0,0]`; other requests use the exact in-bounds square
`[left,top,size]`. Manual mode copies/analyzes only that square and never scans or
normalizes the surrounding frame. Original samples are not modified. The crop is
owned internally, and the selected ROI is returned without cropped pixels.

| Status | Outputs |
|---|---|
| 0 | ROI and all summary measurements valid; correlation can be NaN when undefined. Sector validity is individual. |
| -1 | Invalid argument/ROI: ROI zero, all 16 summary measurements NaN, all sectors invalid. |
| -3 | Auto detection failed: same invalid outputs. Manual mode never returns -3. |
| -4 | Insufficient reliable measurements or resource failure: selected ROI retained if selection completed; all summary measurements NaN, all sectors invalid. |

Non-null outputs are initialized even if another pointer is null. Exception:
detected overlapping storage returns -1 **before any writes**, leaving outputs
untouched and invalid (also protects the source). No C++ exception crosses the ABI.

## Fixed array indexing

Centers are relative to the selected crop; phase satisfies
`q/P + phase = integer` at bright rings, with phase in [0,1).

| Summary index | Measurement | Units |
|---|---|---|
| 0 | ring_phase | cycles [0,1) |
| 1 | center_x | crop pixels |
| 2 | center_y | crop pixels |
| 3 | finesse_proxy | squared-radius spacing / median measured ring FWHM_q |
| 4 | brightness_Ax | signed dimensionless |
| 5 | brightness_Ay | signed dimensionless |
| 6 | linewidth_Wx | signed dimensionless |
| 7 | linewidth_Wy | signed dimensionless |
| 8 | brightness_magnitude | hypot(Ax,Ay) |
| 9 | brightness_direction_deg | degrees [0,360) |
| 10 | linewidth_magnitude | hypot(Wx,Wy) |
| 11 | linewidth_direction_deg | degrees [0,360) |
| 12 | brightness_width_correlation | Pearson r [-1,1]; NaN if either sum of squared deviations <= 1e-12 |
| 13 | valid_ring_count | integer count stored in DBL |
| 14 | valid_sector_count | integer count stored in DBL |
| 15 | harmonic_fit_RMS | dimensionless, formula below |

For `s=0..15`, sector center is `theta=2*pi*s/16`, covering +/-11.25 degrees.
0 degrees is right, 90 down, 180 left, 270 up. Positive width components point
toward **broader** fringes; they are never negated to follow brightness.
Directions are atan2(Y,X) mapped into [0,360); near zero magnitude the direction
is unstable and should not be used as a physical signal.

| Flat index | Value |
|---|---|
| 4*s+0 | normalized local-baseline-subtracted prominence |
| 4*s+1 | normalized FWHM_q |
| 4*s+2 | number of matched rings contributing to this sector |
| 4*s+3 | valid flag: 1 or 0 |

Invalid sector is exactly `[NaN,NaN,0,0]`. The row-major 64-element output may
be reshaped into **16 rows x 4 columns**; keep the field dimension last.

## Numerical procedure and quality limits

1. Reuse the existing reference gradient-based subpixel center estimator and
   squared-radius radial-profile method for phase, spacing and a consecutive
   sequence of ring identities. This initialization requires three bright rings;
   at least two must survive the additional shape/coverage checks.
2. For every initialized ring, require the complete circle plus both trough
   neighborhoods to fit inside the crop (two-pixel margin). Sample nine distinct
   rays in each sector, 144 rays in total, with bilinear interpolation and radial
   step at most 0.25 pixels. Analyze rays individually before angular aggregation.
   This avoids broadening caused by averaging displaced peaks in a wide sector.
   Only a small reusable ray buffer is retained; no 2D nonlinear Airy fit is run.
3. Locate the maximum only within +/-0.20P of that physical ring's initialized q.
   These nonoverlapping peak gates prevent n/n+1 swaps; peaks outside the gate
   are rejected rather than reassigned. Use parabolic subpixel peak interpolation
   and the mean of adjacent trough minima in the +/-0.52P neighborhood as baseline.
   Prominence is peak minus that baseline. Interpolate both half-prominence
   crossings linearly in radius and use **r_right^2-r_left^2** for FWHM_q (no
   thin-ring approximation).
4. Reject unresolved radial widths <2 pixels, FWHM_q >0.55P, prominence <3 in
   the existing affine analysis scale, peaks at the search boundary, incomplete
   half-height crossings, highly unequal troughs (>0.30 prominence), raw U16
   saturation >=65534, and unstable within-sector widths (median relative
   deviation >0.15). Flat saturated plateaus also fail the curvature check.
   Saturation below full U16 range is not universally identifiable without camera
   metadata: broad clipped tops can still bias measurements. Check exposure.
5. A sector/ring requires >=6/9 valid rays; combine their prominence and widths
   separately with medians. A ring requires >=10/16 sectors and no circular gap
   of more than four sectors. Normalize each ring's prominence and width by their
   respective arithmetic means over that ring's valid sectors. Combine rings
   with a median per sector, requiring >=2 rings and median maximum absolute
   prominence/width disagreement <=0.20. Repeat the angular coverage check.
6. Fit `p_s=b_p+Ax*cos(theta_s)+Ay*sin(theta_s)` and
   `w_s=b_w+Wx*cos(theta_s)+Wy*sin(theta_s)` independently using six Huber IRLS
   solves, including intercepts. Initial sector weights are equal. Huber scale
   is max(0.015, 1.4826*median absolute residual), threshold 1.5 scales. Missing
   sectors are omitted. Intercepts are not forced to one, especially important
   with partial angular coverage. Normalization is over available sectors;
   selective missing data can bias absolute coefficient amplitudes.

With V the final valid sectors and N=|V|, the reported RMS is exactly

```
sqrt(sum(s in V, (p_s-b_p-Ax*cos(theta_s)-Ay*sin(theta_s))^2
                +(w_s-b_w-Wx*cos(theta_s)-Wy*sin(theta_s))^2) / (2*N))
```

It uses **unweighted final residuals**, despite robust fitting. RMS >0.18 is
rejected. It is a fit diagnostic, not calibrated statistical uncertainty.
Pearson correlation uses paired final sector profiles with their arithmetic
means, not harmonic predictions. Finesse uses the median across accepted rings
of each ring's arithmetic mean valid-sector width; it follows the existing
phase/center/effective-finesse convention, but is independently measured.

Pixel interpolation broadens very narrow peaks and attenuates measured width
modulation. The two-pixel cutoff limits but does not eliminate this bias. No
pixel-response deconvolution or optical linewidth calibration is performed.
Geometry changes within a ray or a wrong initialized ring identity can cause
rejection. The first harmonic cannot describe all distortions: modest ellipticity
can return success with low first-harmonic RMS, so neither zero coefficients nor
low RMS establish circularity or good alignment. Full-circle selection discards
partly clipped outer rings. Severe obscuration or high finesse can leave too few
resolved rings. See the measured validation report for the tested parameter range.

## LabVIEW 32-bit and 64-bit integration

Use an architecture-matching DLL. The permanent public files remain
`FPAnalyzer.dll` and `FPAnalyzer.h`; use the same plain C header with the Import
Shared Library Wizard. Importing the header is not proof of correct allocation.
**Unsized `[]` arrays do not specify or allocate buffer lengths. Even fixed C
annotations `[3]`, `[16]`, `[64]` would not guarantee that the Wizard creates
arrays of those lengths.** No existing VI is changed by this feature.

If the Wizard still marks the export unrecognized, create a new VI, place a
**Call Library Function Node** from Connectivity > Libraries & Executables,
and open Configure. Select the matching-architecture `FPAnalyzer.dll` directly
and enter/select `FP_AnalyzeFringeAsymmetryU16`. This manual route does not need
the Wizard to parse any header. For **32-bit LabVIEW on Windows 11 x64**, select
the **x86 DLL**; the operating system bitness does not determine DLL bitness.
Define the return and all seven parameters manually using the table below.

Configure a Call Library Function Node as follows, in exactly this order:

| Parameter | CLFN configuration |
|---|---|
| Return | Numeric, signed 32-bit integer, value |
| image_pixels | Array, unsigned 16-bit integer, 1 dimension, Array Data Pointer, input |
| image_width | Numeric, signed 32-bit integer, value |
| image_height | Numeric, signed 32-bit integer, value |
| roi_in | Array, signed 32-bit integer, 1 dimension, Array Data Pointer, input |
| roi_out | Array, signed 32-bit integer, 1 dimension, Array Data Pointer, output |
| asymmetry_out | Array, double, 1 dimension, Array Data Pointer, output |
| sector_out | Array, double, 1 dimension, Array Data Pointer, output |

Select function name `FP_AnalyzeFringeAsymmetryU16` and **C/cdecl**, not stdcall.
Set minimum sizes to 3, 3, 16 and 64 for those four arrays where supported; set
image minimum size to width*height. These settings supplement explicit allocation.
All per-call state is local; concurrent calls with disjoint buffers are supported.
An initial wrapper may use the UI thread until integration has been checked.

Create a new wrapper VI with these steps:

1. Check positive width/height and product <=2147483647 using a wider integer
   during multiplication. Array Size(image) must equal width*height. Convert a
   native 2D camera image to row-major 1D U16 in the established camera convention.
   Keep effective 8/10/12/14-bit samples unchanged in U16 storage.
2. Normalize an empty ROI input to an allocated I32 `[0,0,0]`; reject other
   lengths except exactly 3. Validate manual bounds without adding overflowing
   coordinates. Do not pass an empty array to the DLL.
3. Use **Initialize Array** with an I32 zero and dimension size **3** for roi_out.
   Use a DBL NaN constant and dimension size **16** for asymmetry_out. Create a
   separate DBL array with dimension size **64** for sector_out. Wire all three
   allocated arrays into the input side of the CLFN output-array terminals.
   Keep roi_in separate from roi_out. Do not reuse/alias output storage.
4. Before the call, use Array Size to check lengths 3/16/64 and roi_in length 3.
   If the generated node allocates 6/8 or any other sizes, correct its wiring;
   do not grow the DLL output buffers or change its signature to compensate.
5. After the call, check Array Size again: roi_out=3, summary=16, sector=64.
   Wire status into an error case structure. Only status 0 enables numerical
   results. On -4 ROI may remain useful, but measurements are invalid. Accept
   NaN correlation on success; check each sector valid flag before plotting.
6. Optionally Reshape Array(sector_out) to dimensions 16,4; verify row s has
   indices 4*s through 4*s+3. Label Ax/Ay and Wx/Wy in camera coordinates.

Do not resize Array Data Pointers with C allocators. No LabVIEW runtime or memory
manager is required. Wizard import, generated VI wiring, and acquisition-loop
operation must still be verified manually in actual LabVIEW on each architecture.

## Scientific interpretation

These are signed **image asymmetry** measurements, not mechanical tilt angles.
Brightness direction need not equal a mirror tilt axis. Illumination, optical
aberrations, camera defocus and Z displacement can affect the result; zero
asymmetry need not locate optimum parallelism. A future controller needs an
independently established optimum, reference vectors, and experimental X/Y
voltage calibration. An eventual error can be current minus reference asymmetry.
This feature issues no actuator commands and performs no voltage calibration.

## Reproducing validation

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
# Linux examples; Windows binaries are under build/Release with .exe/.dll:
build/fp_asymmetry_tests
build/fp_asymmetry_benchmark build/asymmetry_benchmark.csv
python examples/asymmetry_ctypes_test.py build/libFPAnalyzer.so
python examples/fringe_asymmetry.py build/libFPAnalyzer.so data/test_frames
```

The experimental validator expects exactly Image0803.bmp, Image0792.bmp,
Image0699.bmp and Image0855.bmp. It emits per-image JSON including all summary
and sector outputs; NaN is JSON null. Exit 77 means missing inputs (CTest skip),
1 means an available file failed, 0 means all four ran successfully. It loads
uncompressed grayscale indexed 8-bit or grayscale 24/32-bit BMPs without scaling
(255 remains 255 in U16), verifies grayscale channels/palettes and preserves row
orientation. Unsupported/color BMPs are explicitly rejected rather than silently
converted. The DLL itself supports the full native U16 dynamic range. Do not
commit camera images without permission. Earlier exploratory first-ring values
are qualitative context, not fitted numerical targets or calibrated ground truth.
