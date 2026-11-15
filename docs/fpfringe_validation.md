# FPFringe API 3: ROI extraction validation, 2026-11-15

## Original failure and scope

The fetched branch contained two user commits beyond 953d51a: f11698f had
already removed a strict post-fit full-frame fringe-evidence gate; c6e3e2e
changed the public header to a crop/capacity prototype but left an extra center
output. The implementation, private function, clients and tests still used the
old six-argument geometry/center interface. This mismatch prevented a coherent
build. The old automatic sizing also selected the largest centered frame square,
which could surround compact fringes with excessive dark background.

The earlier post-fit evidence gate could reject a successfully fitted local
pattern; this refactor keeps that gate removed. Image0180.bmp was not available,
so its reported failure and improvement cannot be independently verified here.
The new local fit and extent-based sizing address the design issues without
changing phase/finesse/asymmetry algorithms.

## Implemented API and localization

FPF_GetApiVersion is now 3. The public ROI function has exactly seven parameters:
source U16, width, height, requested side, output U16 crop, capacity in U16
**elements**, and I32[3] [left,top,side]. The fitted center stays internal.
There are still exactly four undecorated FPF exports; the checker remains absent.
The new plain-C header contract test guards the complete seven-argument prototype.

Candidate detection reuses the existing full-frame contrast/component detector.
Center fitting uses locally normalized private candidate crops, retrying sides
1, 1.5 and 2 times the original candidate side if fitting fails (bounded by the
frame and skipping duplicate capped sizes). Successful fitting immediately ends
retries; no strict ring count, peak prominence or analysis-success gate follows.
The final crop copies original U16 rows without resampling or intensity changes.

For initial candidate side D, midpoint (bx,by), fitted center (cx,cy), automatic
sizing uses E=D+2*max(abs(cx-bx),abs(cy-by)), a per-side margin max(4,0.05*E),
and S0=max(24,ceil(E+2*margin)). The final side is capped by the largest centered
square that fits the frame, but S0 is the desired size rather than that maximum.
Offsets are nearest valid integers with <=0.75-pixel center displacement per
axis. Exact positive sizes are never silently resized.

Status -2 returns valid geometry and leaves every crop pixel untouched; callers
can allocate side*side and retry on the same frame. Other failures initialize
ROI to zero and leave pixels untouched. Known overlap returns -1 before any
writes. Full parameter order, formulas, statuses and LabVIEW allocation guidance
are in [the API guide](fpfringe.md).

## Synthetic and ABI validation

Linux x86_64, GCC 14.2.0, Release: 19 CTest entries passed, three optional image
entries skipped for missing BMPs. ASan+UBSan RelWithDebInfo: 13 passed, one image
entry skipped, no sanitizer findings. These are executed entries, not zero-test
runs. Existing legacy tests and their numerical tolerances remain unchanged.

The new dedicated ROI suite covers centered/off-center patterns, square,
landscape and portrait images, compact 1280x960 frames, bright centers with weak
outer rings, a single-ring geometric-fit case, automatic/exact/1x1 sizes,
centering and frame bounds, every returned U16 sample, output canaries and tail,
zero/short/exact capacity and retry, nulls, negative capacity, invalid dimensions,
overflow, impossible geometry, absent candidates, unreliable center fits,
known/partial overlaps and eight concurrent independent extractions.

[Measured synthetic geometry](benchmarks/2026-11-15-linux-roi-extraction.txt):

| Synthetic frame | True center | Returned [left,top,side] |
|---|---|---|
| 512x512 | (255.3,255.7) | [84,84,344] |
| 640x480 | (480.3,220.7) | [325,65,312] |
| 480x640 | (220.3,420.7) | [63,263,316] |
| 1280x960, compact | (659.0,469.4) | [481,291,357] |

All samples in successful crops match the corresponding original U16 source
values exactly. Center placement is checked against synthetic truth within
0.75 pixel per axis; compact/weak patterns produce crops below 512 pixels wide,
not near-full-frame squares. A single ring can be located without requiring
three peaks for subsequent reference-method success.

The end-to-end test extracts the compact 1280x960 fixture, then passes exactly
the returned square to all A/B/C/D selectors and the asymmetry function. All
succeed and match legacy calls supplied with the identical square samples,
including all 16 summary and 64 sector outputs and paired NaNs.

The pre-refactor FPFringe API-2 library was retained outside the checkout.
Direct old/new analysis comparisons on five effective bit depths and square,
landscape and portrait images gave **672 exact output values and 35 identical
statuses**, including failures/NaNs. Both retained analysis bodies and all shared
Analysis/Methods/Asymmetry code are unchanged. The existing ctypes suite also
passes 480 old-FPAnalyzer/new-FPFringe output comparisons and verifies API 3,
the removed symbol's absence, crop canaries and exact source pixels.

## Release performance

[Full measured CSV](benchmarks/2026-11-15-linux-fpfringe.csv). The benchmark keeps
ROI/A/B/C/D/asymmetry on 320x320, 640x480 and 1280x960 fixtures and adds compact
and weak-outer-ring 1280x960 ROI cases. It records pattern and returned side.
Two warm-ups and 21 measured calls per row: 420 successes, zero failures across
20 rows. Rendering and caller buffer allocation are outside timing; ROI timing
includes localization, local fitting, capacity validation and raw pixel copying.
Median is index 10, nearest-rank p95 index 19. Results below are Linux x86_64
Release observations, not fabricated Windows timings or a hard real-time claim.

| Frame | Pattern | Crop side | Median ms | p95 ms | Success/failure |
|---|---|---|---|---|---|
| 320x320 | wide | 319 | 5.088 | 5.851 | 21/0 |
| 640x480 | wide | 479 | 10.200 | 10.943 | 21/0 |
| 1280x960 | wide | 959 | 33.491 | 34.945 | 21/0 |
| 1280x960 | compact | 357 | 4.087 | 4.181 | 21/0 |
| 1280x960 | weak_outer | 403 | 4.725 | 5.237 | 21/0 |

Large frame-spanning patterns can legitimately reach the centered boundary cap;
compact patterns use the detected extent. No phase/finesse/asymmetry speedup is
claimed: their numerical implementations were not changed. Shared-host timing
variability prevents treating these numbers as a calibrated performance gain.
Windows runners record the same CSV and job-summary table per architecture.

## Windows packaging and remaining manual checks

The existing Windows Win32/x86 and x64 matrix builds and tests both DLLs with
matching Python bitness, validates exact exports/architecture and no runtime
FPAnalyzer dependency, checks installed header hashes and uploads separate
FPFringe-windows-x86/x64 and retained FPAnalyzer-windows-x86/x64 packages.
Use the workflow associated with the pushed commit for its actual Windows
outcome; Linux results do not establish MSVC execution. Each test-report artifact
contains the measured CSV and complete CTest log.

No experimental BMPs, including prioritized Image0180.bmp, are present in the
workspace. The compact synthetic fixture is representative, **not Image0180**
and not fabricated experimental evidence. Its coordinates are not tuned to force
the old diagnostic ROI. The updated optional validator accepts Image0180 plus
the five earlier frames, checks source pixels, and compares old/new analyses on
the DLL-returned crop. Available-frame ROI failures produce a failing status;
missing files are explicitly skipped.

No actual NI Wizard/CLFN execution or camera acquisition was performed. On the
user's PC, replace the old center-output node with the exact seven-parameter API
3 signature, allocate I32[3] and caller-owned U16 storage, pass capacity in
**pixels**, handle -2 without using crop pixels, trim/reshape side*side only on
success, and use x86 with 32-bit LabVIEW. Verify actual camera ROI increments and
full-frame coordinate offsets. Keep camera tracking ROI fixed between deliberate
initialization/reacquisition operations. Multi-pattern/glare scenes, weak rings
below the contrast detector and fits that remain unreliable after all bounded
retries remain experimental limitations.

## Changed files

- include/FPFringe.h; src/FPFringe.cpp; src/FringeDetection.cpp/.h.
- CMakeLists.txt; tests/test_fpfringe_roi.cpp (new); tests/test_fpfringe.cpp;
  tests/test_fpfringe_c.c; tests/check_fpfringe_header.cmake;
  tests/benchmark_fpfringe.cpp.
- examples/fpfringe_ctypes_test.py; examples/fpfringe_validate.py.
- .github/workflows/windows-build.yml (pattern/ROI-side timing summaries).
- README.md; AGENTS.md; docs/fpfringe.md; this report.
- docs/benchmarks/2026-11-15-linux-fpfringe.csv and
  docs/benchmarks/2026-11-15-linux-roi-extraction.txt (new measurements).

All legacy headers, exports, algorithms and tests are unchanged. The FPF export
names remain unchanged; only the ROI function's ABI and API version are revised.
