# Fringe-asymmetry validation, 2026-11-14

## Current machine

Linux x86_64, GCC 14.2.0, C++17. CMake Release build. All 12 runnable CTest
entries passed; two experimental-image entries were explicitly skipped because
camera BMPs are absent. Existing analysis/selector accuracy, regression,
concurrency, C/C++ ABI and ctypes tests pass without relaxed tolerances or
changes to their numerical implementations. The added native suite checks
signed independent harmonics, center displacement, phase, effective bit depth,
noise, background/illumination, saturation, missing sectors, geometry, exact
nonzero manual ROI, source preservation, output canaries, nulls, overflow,
overlap rejection, failure NaNs and eight concurrent independent calls.

ASan+UBSan RelWithDebInfo: eight runnable CTest entries passed, one optional
experimental-image entry skipped, no sanitizer findings. Linux exported-symbol
inspection found exactly the original three symbols plus
FP_AnalyzeFringeAsymmetryU16. Installed-package header/name checks passed.

## Synthetic accuracy

The independent renderer uses circular Airy-like rings with exact local
trough-relative FWHM_q = 0.24P*(1+Wx*cos(theta)+Wy*sin(theta)). The standard
320-pixel ROI has center (153.3,164.7), P=3600, phase=0.27, 60% U16 amplitude
and 5% constant background. No synthetic image is substituted for an unavailable
experimental BMP. Complete measured output is retained in
[the accuracy log](benchmarks/2026-11-14-linux-asymmetry-accuracy.txt).

| Case | Imposed Ax,Ay | Imposed Wx,Wy | Measured Ax,Ay | Measured Wx,Wy |
|---|---|---|---|---|
| Symmetric | 0,0 | 0,0 | -0.00118,0.00135 | 0.00040,-0.00027 |
| Displaced center (132.4,181.2) | 0,0 | 0,0 | 0.00057,0.00039 | -0.00496,0.00202 |
| Brightness only | 0.30,-0.20 | 0,0 | 0.29950,-0.20071 | -0.00036,0.00038 |
| Width only | 0,0 | -0.22,0.18 | -0.01127,0.00947 | -0.19946,0.16420 |
| Combined opposite | 0.30,-0.20 | -0.22,0.18 | 0.28709,-0.19030 | -0.20000,0.16503 |
| Combined independent | 0.30,-0.20 | 0.15,0.12 | 0.30745,-0.19162 | 0.13320,0.10880 |
| Reversed directions | -0.21,0.28 | 0.16,-0.18 | -0.20222,0.27129 | 0.14291,-0.16093 |

A spatial illumination gradient plus background slope gives Ax=0.27108,
Ay=-0.16927 while Wx=-0.00073, Wy=0.00069. Thus an amplitude gradient was
not interpreted as substantial width asymmetry in this tested case.
Across five symmetric center placements, fitted-center errors reach 0.338 pixels,
yet absolute linewidth coefficients remain <0.005. Narrow-ray measurement prevents
sector-average smearing despite these initialization errors.

For imposed (Ax,Ay,Wx,Wy)=(0.30,-0.20,0.15,0.12), phases
0.05/0.27/0.53/0.85 give respective coefficient spans approximately
0.00268/0.00508/0.00416/0.00110. The largest absolute error in that sweep is
0.01680. This quantifies residual phase bias for this spacing/width/ROI range;
it is not a guarantee for arbitrary finesse or geometry.

Effective 8/10/12/14/16-bit data were tested at 45% intensity, 15% background
and Gaussian noise sigma=0.2% of each camera range. All retain correct signs;
maximum coefficient error is <0.019. The suite also checks 256/480-pixel ROIs,
a weak outer aperture, obscuration with 15 remaining valid sectors, and mild
saturation leaving two reliable rings and 13 sectors. Strong obscuration,
severe ellipticity, near-zero contrast and too few rings return -4. Moderate
ellipticity (0.025) can return success with RMS 0.01190: the first harmonic
**does not validate circularity**. A limited aperture still yields three rings.

The default absolute per-coefficient acceptance tolerance is 0.04; symmetry
and amplitude-only false-width cases have tighter limits (0.025--0.03), and
the spatial-gradient width-vector magnitude must be <0.025. Mild saturation
uses 0.05. Existing pre-feature tests retain their original tolerances.

Width modulation is attenuated by roughly 9% in the width-only example,
consistent with bilinear pixel interpolation broadening. Small induced brightness
coefficients also remain. The cutoff on measured radial widths (<2 pixels)
rejects strongly unresolved peaks but is not a deconvolution. Quantitative use
near the resolution limit needs camera/optical calibration or better sampling.

## Release timings

[Measured CSV](benchmarks/2026-11-14-linux-asymmetry.csv), 64-bit Linux,
three warm-ups and 31 measured calls, no failures. Median is sorted index 15;
p95 uses nearest rank ceil(0.95*31), index 29. Timed scope is the complete C
ABI call including internal crop, normalization, detection (auto only), analysis,
and outputs. Synthetic rendering is outside timing. Manual and automatic modes
use exactly the same selected ROI. No before/after speedup is claimed.

| Frame | Selected square | Mode | Median ms | p95 ms |
|---|---|---|---|---|
| 256x256 | 242 | manual | 3.537 | 4.194 |
| 256x256 | 242 | auto | 3.998 | 4.405 |
| 320x320 | 320 | manual | 5.547 | 5.949 |
| 320x320 | 320 | auto | 6.221 | 6.669 |
| 640x640 | 640 | manual | 21.476 | 24.432 |
| 640x640 | 640 | auto | 22.612 | 23.832 |

These are observations on a shared cloud CPU, not an acquisition-loop timing
guarantee. Windows runners successfully ran the same Release benchmark independently for x86
and x64 for implementation commit `07f37c5591be4722ca95425179d4ffe6acb80d54`. Their actual CSV and test logs are uploaded as test-report artifacts;
measured timing tables are also written to each Actions job summary. Actual
Windows CSV values are retained in those artifacts; they could not be retrieved
through the available unauthenticated public web view/API, so no Windows numeric
timings are transcribed here. Do not substitute Linux measurements for Windows
timings or confuse total CI job duration with per-frame benchmark latency.

## Experimental and integration limitations

None of Image0803.bmp, Image0792.bmp, Image0699.bmp, Image0855.bmp is available
in the workspace. Their results, multiple-ring direction consistency and the
comparison with earlier exploratory observations therefore remain **unmeasured**.
The ready-to-run command is in [the feature documentation](fringe_asymmetry.md#reproducing-validation).

The cloud machine has no MSVC/Windows or LabVIEW runner. Windows validation
was performed by the remote GitHub Actions matrix instead. The
[PR workflow run for implementation commit 07f37c5](https://github.com/Mik-Poch/Fabry-Perot-Control/actions/runs/43618887578)
completed **Success**. Public job pages independently confirmed every step
successful for [Windows x86](https://github.com/Mik-Poch/Fabry-Perot-Control/actions/runs/43618887578/job/132519862825)
and [Windows x64](https://github.com/Mik-Poch/Fabry-Perot-Control/actions/runs/43618887578/job/132519862687):
MSVC Release compilation, native/C ABI/ctypes tests, exact undecorated export and
architecture verification, installed package/header verification, and uploads.
The run contains FPAnalyzer-windows-x86 and FPAnalyzer-windows-x64 artifacts
with FPAnalyzer.dll and FPAnalyzer.h, plus FPAnalyzer-test-report-x86/x64
containing test logs and measured benchmark CSVs. These results were verified
from the public GitHub web pages despite GitHub API returning Forbidden. Local
Linux results and remote Windows results are distinct; no actual LabVIEW run
was available.

No existing VIs were modified. Actual Wizard import, generated array allocation,
new wrapper VI operation in 32-bit/64-bit LabVIEW and sustained camera acquisition
remain manual checks. The detailed CLFN/wrapper instructions specify explicit
3/16/64 preallocation and post-call size checks. No actuator calibration,
mechanical tilt claim or closed-loop regulation is implemented or validated.
