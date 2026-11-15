# Four methods: definitions, evidence and limits

Feature generation v3 keeps ABI version **2** and the original U16 function.
The new selector is additive; discover it by symbol lookup. All implementations
are single-frame, CPU-only C++17, without global mutable state or external
numerical libraries. The original function always selects B. Failed C/D fits
return -4, never their B initializer's metrics. Invalid selector returns -5.

## Common processing and definitions

Pipeline.cpp validates disjoint caller buffers and I32 size arithmetic, selects
an exact manual ROI or detects an automatic square, copies original U16 values,
then derives crop-local 1st/99th percentiles with a 65536-bin histogram. Samples
are viewed in double precision; camera data is never quantized to U8 or modified.
Manual mode has no full-frame statistics/downsampling/detection. C/D/A/B all
receive the same normalized crop. A/B/C/D numerical code is in Methods.cpp and
Analysis.cpp; diagnostics and optional stage clocks belong to the invocation.

Bright rings satisfy **q/P + phase = integer**, q=(x-cx)²+(y-cy)².
Phase is modulo one, center is relative to the crop, and finesse is squared-
radius spacing divided by trough-relative FWHM. For the Airy model with
coefficient F this definition is **pi/(2*asin(1/sqrt(F+2)))**. It differs from
zero-baseline or intrinsic-cavity finesse definitions. These outputs are not
absolute mirror separation, mechanical tilt, actuator calibration or control.

## B (1): preserved radial reference

The two separable [1 2 1]/4 smoothing passes, gradient thresholds, robust
intersection-of-gradient-normals center fit (eight iterations), complete-circle
squared-radius profile, prominent consecutive peaks, linear ring-order fit and
FWHM definition are retained. The optimization uses two reusable smoothing
buffers, reserves gradient storage and precomputes accepted gradient weights
and normal equations' constant terms. The original floating operation order
for center residuals is preserved: changing it exposed peak-selection sensitivity
in a low-bit-contrast fixture. No iterations/thresholds were removed for speed.

Six noiseless four-metric baselines were recorded from **a9a0316 before changes**,
including wrap phases, offset center, low-bit contrast and broader peaks. The
native test compares against those values with existing equivalence tolerances
(0.1 pixel, 0.005 cycle, 5% finesse); the recorded Linux optimized outputs agreed
within 1e-8. Original/new-selector B calls additionally require exact equality.
Original truth, failure, buffer-capacity and U16-preservation tests still run.

A pre-optimization native stage profile (1280×960, ROI 726², radius 340, P=15000,
amplitude 50000, background 1000, **noise=0**, two warm-ups then ten calls) found
mean times: ROI including frame normalization 2.061 ms, copy 0.064 ms, crop
normalization 0.363 ms, center **28.012 ms**, profile 1.786 ms, peaks 0.033 ms.
This identified center fitting as the main cost. Current stage reports use
noise=3 and median/p95; do not subtract those different summary statistics.

## C (2): robust 2D Airy fit

B supplies initial center, P, phase and linewidth. Resolved radial peaks define
a supported complete disk, ending 0.15*P before the last peak to reduce aperture
taper contamination. The fit uses **individual 2D pixels** inside that disk,
not a fit to the 1D average:

I = B + bx*X + by*Y + A*(1+ax*X+ay*Y)/(1+F*sin²(pi*(q/P+phase))).

X/Y are fixed coordinates normalized by crop size. The background plane and
linear illumination slopes model moderate intensity inhomogeneity; arbitrary
aperture/PSF/geometric distortion is not modeled. There are 11 parameters.
Analytic derivatives form a Huber-weighted Gauss–Newton system; a bounded
Levenberg–Marquardt damping loop accepts only decreasing robust objectives.
A/P/F are log-parameterized, amplitude is positive, F is bounded [0.1,500],
illumination slopes [-1,1], and center stays within four pixels of the seed.
P stays within exp(±0.06) of the seed and unwrapped phase within ±0.2 cycles,
limiting false-order minima. Solutions at key parameter bounds are rejected.

Two spatial sampling grids (coarse ~size/90, fine ~size/180, at least 300 pixels)
reduce cost. Each optimization has a 45-iteration limit and explicit small-step,
objective-improvement or gradient convergence checks. Nonconvergence, inadequate
support/amplitude or robust RMS >0.12 of the fitted-region contrast return -4.
The reported finesse is derived analytically from fitted F with the common
trough baseline. Diagnostics retain relative robust RMS and iteration count.
The normal equations use a small pivoted linear solve, no numerical dependency.

A good B seed and several well-sampled rings are prerequisites. This is a
circular, single-pattern model, not a universal optimizer. The ideal synthetic
suite favored C in accepted cases; that is not evidence it is always more
precise on real images or images with camera PSF, clipping or optical aberration.

## D (3): eight angular sectors

A B seed defines complete-circle q bins, split into eight 45-degree sectors.
Each sector independently measures peak q positions, phase, spacing and FWHM.
Sectors with unreliable profiles or spacing/phase inconsistent with the common
seed are excluded; at least five sectors and 15 resolved peaks must remain.

For guessed center c and true displacement delta, q_true along direction u is
q - 2*sqrt(q)*(delta dot u) + |delta|². Therefore sector peaks obey

q_s = P*n - P*phase - |delta|² + 2*sqrt(q_s)*(delta dot u_s).

A robust four-parameter fit estimates P, common intercept and signed X/Y
corrections. u_s uses the mean direction of a sector, including its sinc angular
factor. Integer ring orders are assigned near the reference solution: no
arithmetic average of raw phases near 0/1 is taken. Two center/profile refinements
are used; large corrections, inconsistent spacing or RMS residual >0.04 cycles
are rejected. Median accepted-sector finesse uses the common measured FWHM.
Diagnostics retain sector validity, phases and dimensionless directional
residuals for future calibration. They are **not mechanical tilt angles**.

The displacement model is a small-correction approximation initialized near
the center. It does not promise recovery of large offsets from an arbitrary
seed or severe ellipticity. Low-contrast/missing sectors can reduce support
below the rejection threshold. It retains the cost of B initialization.

## A (0): opposite-spoke correlation

A starts at the crop's geometric center without full-image gradient fitting.
It bilinearly samples **eight spokes** forming four opposite pairs. For a
center displacement delta, the signed best radial lag of an opposite pair is
2*(delta dot u); solving the four directional projections yields X/Y movement.
Three recentering rounds use bounded integer-lag search followed by interpolated
subpixel searches. The initial range is min(0.35*size, available radius-24) in
lag pixels. This is finite capture range, not arbitrary center recovery.

Normalized cross-correlation must exceed 0.65, with initial peak separation
from competing lags >0.04. Pair support, consistency and final displacement
checks reject ambiguous/poorly sampled results. A light squared-radius profile
then computes **both phase and finesse** with the common peak analyzer. Large
crops use a Cartesian stride max(1,size/350), reducing samples; B always uses
stride 1. Crop normalization/copy are still included in the DLL timing.

A can miss features between spokes, be fooled by competing patterns or fail on
large offsets/dense unresolved rings. Subpixel center does not imply arbitrary
precision: bilinear interpolation and sparse sampling can bias sharp fringes.
It has no frame history or guaranteed tracking speed. The reported speedups
below measure the complete four-metric estimator, not center-only timing.

## Reproduce the measurements

Build Release, then run:

```sh
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure --verbose
./build/fp_methods_benchmark 21 build/methods_benchmark.csv
./build/fp_b_compare /absolute/path/to/previous/libFPAnalyzer.so
```

On Windows use build/Release executables and a previous DLL of matching bitness.
For a repeatable B comparison use the DLL from source a9a0316, or build that
commit in a separate source archive (do not reset a working checkout).
fp_b_compare alternates previous/current calls on identical generated images
and ROI, two warm-ups and 25 samples/version, checking successful matching ROI.
It does not load an arbitrary v1 library with a different signature.

fp_methods_benchmark uses 1280×960 U16, center (645.4,470.8), phase .27, F=24,
amplitude 50000, background 1000, noise sigma 3. Radius/P pairs are 110/1800 and
340/15000. A B auto call chooses ROI [502,326,277] or [293,101,726], then **all
methods use that same ROI**. Two warm-ups and 21 measured calls per mode report
median, nearest-rank p95, min/max and success/failure counts. The native timer
covers the exported DLL call, including allocation/normalization/analysis;
image generation and caller buffer allocation are outside it. Four metrics are
checked against synthetic truth. Separate instrumented internal calls report
ROI (including full-frame normalization), copy, crop normalization, center,
profile, peaks and nonlinear/sector fitting. These stage medians are not
additive. Manual ROI stage time is essentially the selection branch, no scan.

CI runs Release benchmark and accuracy tests on **both Win32 and x64**, with
CSV and verbose LastTest.log in each test-report artifact. Linux Release and
ASan/UBSan tests provide complementary checks, not Windows or LabVIEW proof.

## Recorded native Linux results (2026-11-13)

GCC 14.2, x86_64 shared cloud, CMake 3.31.10, Ninja 1.13.0. Timings vary with
host load and sampling; these are observations, not real-time guarantees.
[Raw method and stage CSV](benchmarks/2026-11-13-linux-methods.csv).

| Method | 277² manual median / p95 | 277² auto median | 726² manual median / p95 | 726² auto median |
| --- | --- | --- | --- | --- |
| A | 0.946 / 1.258 ms | 3.117 ms | 3.547 / 3.932 ms | 5.446 ms |
| B | 4.539 / 6.081 ms | 7.073 ms | 30.030 / 33.086 ms | 31.857 ms |
| C | 14.960 / 17.324 ms | 17.339 ms | 37.767 / 41.725 ms | 39.121 ms |
| D | 5.517 / 6.149 ms | 7.954 ms | 36.086 / 42.363 ms | 40.045 ms |

All **336 timed DLL calls succeeded** (21 per method/mode/radius); the separate
instrumented calls also succeeded. A was 4.80× / 8.47× faster than optimized B
on these manual ROIs. Auto overhead includes full-frame work irrespective of
selected method. C/D prioritize different fits, not minimum runtime.

[Same-machine B before/after CSV](benchmarks/2026-11-13-linux-b-before-after.csv)
records interleaved 25-call medians/p95, separately from that table:

| ROI / mode | Previous median / p95 | Optimized B median / p95 | Median reduction |
| --- | --- | --- | --- |
| 277² manual | 5.973 / 6.849 ms | 3.786 / 4.689 ms | 36.6% |
| 277² auto | 8.018 / 9.996 ms | 6.012 / 7.278 ms | 25.0% |
| 726² manual | 44.590 / 53.324 ms | 32.132 / 33.949 ms | 27.9% |
| 726² auto | 46.485 / 50.877 ms | 34.291 / 37.609 ms | 26.2% |

Only these interleaved results compare old/new optimization on the same machine
and images. Do not compare this cloud's native timings to the user's ~50 ms
manual / ~60 ms auto LabVIEW observations as a speedup. Instrument validation
must separately measure native processing, node marshalling, allocation/copy,
reshape and display, as described in labview_integration.md.

## Accuracy and failures

[Raw 76-row synthetic comparison](benchmarks/2026-11-13-linux-accuracy.csv)
contains 19 fixtures × four methods, signed X/Y error, circular phase error and
relative finesse error. Each image has known center/phase/F/P and finite aperture.
It covers phase .005/.995, a center offset of roughly 24 pixels, Airy coefficients
5/20/40, 8/10/12/16-bit levels, a 40000 background with only 64-level contrast,
noise sigma 0.5/1/3/10% of amplitude, background/illumination slopes, ellipticity
1.5/5% and very low SNR. This is one deterministic seed per degraded comparison,
not a statistical confidence study. Separate pure-noise rejection uses three
seeds/method; prior B noise tests remain. All 13 required well-formed fixtures
passed all four methods within 0.7 px, .03 cycles and 25% finesse.

| Method | Success / all 19 | Failed | Max accepted non-elliptical X / Y error | Max phase error | Max relative finesse error |
| --- | --- | --- | --- | --- | --- |
| A | 17/19 | 2 | .0433 / .0214 px | .00312 | 1.54% |
| B | 17/19 | 2 | .1554 / .0297 px | .00349 | 1.52% |
| C | 16/19 | 3 | .00098 / .00500 px | .000647 | .384% |
| D | 15/19 | 4 | .0189 / .0287 px | .00258 | 2.06% |

Maxima exclude elliptical and failed cases: A/B/C each have 16 accepted
non-elliptical fixtures, D has 15. Failure counts are part of the comparison;
they must not be hidden by a ranking of errors on surviving cases. All methods
rejected 5% ellipticity and the low-SNR fixture. At 1.5% ellipticity A/B returned
phase errors .0127/.0156 and finesse biases about -26%/-25%, while C/D rejected
the circular model. D rejected the 10%-noise fixture while A/B/C accepted it.
C achieved lower errors in these ideal modeled cases and rejected more distorted
cases; **no superiority or real-camera accuracy claim** follows from this suite.
Real images, optical PSF, clipping, saturation, multiple patterns and more seeds
remain validation work. No real-camera frames were supplied; optional tests skip.

Native tests also cover exact ROI/crop/metadata writes and capacity canaries,
NaN errors, manual exclusion of exterior image content, API compatibility,
invalid selectors and concurrent deterministic A/B/C/D calls with separate
buffers. Actual LabVIEW Import Wizard/node wiring, 6/8-array correction,
Run in any thread and instrument timings still require manual verification.
