# FPFringe validation and performance, 2026-11-14

## Verified on Linux x86_64

GCC 14.2.0, C++17, Release; Python 3.12 ctypes. The full Release CTest run
passed 18 runnable entries with three optional experimental-image entries
explicitly skipped. Existing legacy analysis, U16/selector regression, concurrency,
public C/C++ header and ctypes tests passed without relaxed tolerances. Dedicated
FPFringe tests exercise five C exports, plain C99 function-pointer signatures,
C/cdecl calls, output canaries, deterministic failure outputs, nulls, overflow,
invalid method IDs, overlap rejection and concurrent independent buffers across
all four processing functions. The old public header and export definitions
are unchanged.

ASan+UBSan RelWithDebInfo: all 12 runnable entries passed, one optional image
entry skipped, no sanitizer findings. The affected FPFringe targets were rebuilt
and rechecked after the final rectangular/angular/ROI fixture additions.
The installed-package checks passed for both public headers. ELF inspection
found exactly five FPF_ exports in FPFringe and the same four FP_ exports in
FPAnalyzer. ldd confirmed no FPAnalyzer runtime dependency of FPFringe.

## Numerical compatibility and geometry

Before refactoring, the built legacy DLL-equivalent shared library from commit
e6b80d0 was retained outside the checkout. The new library was compared with
that frozen binary using the same square U16 samples, all four selector IDs and
all asymmetry summary/sector fields at 8/10/12/14/16 effective bits. All 480
output comparisons were exact (including paired NaNs), and statuses matched.
This is a Linux same-toolchain comparison, not a cross-architecture bitwise claim.
The repeatable command accepts any same-architecture API-2 baseline:

```
python examples/fpfringe_ctypes_test.py FPFringe.dll previous/FPAnalyzer.dll
```

The native suite also compares square outputs against the concurrently built
legacy DLL. No numerical deviations were observed in the checked square cases.
No old threshold, iteration count, selector ID, output definition or public
calling convention was changed. Only private rectangular geometry was generalized,
with legacy square wrappers retaining the earlier arithmetic operation order.

All A/B/C/D methods succeeded on 320x320, 640x480, 1280x960 and 960x1280
synthetic fixtures. Tests require center error <0.7 pixels, phase error <0.035
cycles and positive finesse. Full measured centers/statuses are in
[the accuracy log](benchmarks/2026-11-14-linux-fpfringe-accuracy.txt).
Known signed brightness/linewidth gradients pass on square and 400x320/320x400
inputs (per-component absolute tolerance 0.04). All 16 summary and 64 sector
values are compared on equivalent square inputs. An off-center 640x480 fixture
returns its independently known full-image center, without a hidden square crop.

ROI tests cover zero/positive requested sizes, exact 1x1/normal sizes, automatic
sizing, weak outer ring support, off-center centers, square geometry, bounds,
subpixel full-frame center accuracy and <=0.75-pixel centering tolerance. An
impossible exact centered size fails with zero ROI and NaN center. Automatic
sizing takes the maximum centered square rather than a tight contrast box;
hardware increment rules remain the caller's responsibility.

## Screening evidence and limits

On ten deterministic synthetic fixture classes, observed FP=0 and FN=0:

| Fixture | Expected/observed Boolean |
|---|---|
| resolved rings | 1/1 |
| weak rings with noise | 1/1 |
| one-DN low contrast | 0/0 |
| two strong rings | 1/1 |
| strong noise without rings | 0/0 |
| blank | 0/0 |
| isolated hot pixel | 0/0 |
| stripes | 0/0 |
| fully saturated | 0/0 |
| half-obscured rings | 0/0 |

The two-ring fixture passes screening while method B returns -4, proving the
screen is independent of B's three-ring requirement. Every completed negative
screen returns status 0 with Boolean 0. These fixture outcomes do not establish
real camera false-positive/negative rates. Partially clipped/saturated rings,
low SNR, undersampling and nonconcentric patterns remain limitations. Screening
is optional and does not guarantee phase/finesse/asymmetry success. The new
asymmetry function inherits the earlier resolution/interpolation bias and
first-harmonic geometry limitations; no optical calibration is claimed.

## Measured Release timings

[Full CSV](benchmarks/2026-11-14-linux-fpfringe.csv), 64-bit Linux, two warm-ups
and 21 measured end-to-end C calls per row, rendering excluded. Median is index
10; nearest-rank p95 is index 19. All 504 measured calls succeeded, no failures.
These are measured shared-cloud CPU latencies, not fabricated Windows timings
or a real-time guarantee. Input parameters are in tests/benchmark_fpfringe.cpp.

| Image | Operation | Median ms | p95 ms | Success/failure |
|---|---|---|---|---|
| 320x320 | ROI | 5.281 | 5.612 | 21/0 |
| 320x320 | screen | 4.446 | 4.853 | 21/0 |
| 320x320 | A | 1.072 | 1.166 | 21/0 |
| 320x320 | B | 4.690 | 5.298 | 21/0 |
| 320x320 | C | 20.217 | 37.512 | 21/0 |
| 320x320 | D | 6.383 | 7.882 | 21/0 |
| 320x320 | asymmetry | 5.304 | 5.786 | 21/0 |
| 320x320 | screen+B | 9.065 | 9.413 | 21/0 |
| 640x480 | ROI | 11.515 | 11.890 | 21/0 |
| 640x480 | screen | 11.992 | 12.944 | 21/0 |
| 640x480 | A | 2.333 | 2.552 | 21/0 |
| 640x480 | B | 11.965 | 12.968 | 21/0 |
| 640x480 | C | 28.465 | 32.335 | 21/0 |
| 640x480 | D | 15.431 | 16.834 | 21/0 |
| 640x480 | asymmetry | 13.468 | 14.758 | 21/0 |
| 640x480 | screen+B | 24.447 | 28.297 | 21/0 |
| 1280x960 | ROI | 40.664 | 66.284 | 21/0 |
| 1280x960 | screen | 40.739 | 42.046 | 21/0 |
| 1280x960 | A | 5.753 | 6.058 | 21/0 |
| 1280x960 | B | 42.205 | 50.438 | 21/0 |
| 1280x960 | C | 48.692 | 51.572 | 21/0 |
| 1280x960 | D | 53.289 | 59.911 | 21/0 |
| 1280x960 | asymmetry | 42.803 | 44.086 | 21/0 |
| 1280x960 | screen+B | 84.989 | 87.641 | 21/0 |

Screening plus B approximately doubles valid-frame cost on these fixtures,
because normalization/center/profile work is repeated. For continuous valid
acquisition, calling B directly and using its status is usually preferable.
Screening may still be useful for choosing subsequent work or presenting a
Boolean UI result; no unmeasured speedup on rejected frames is claimed.

## Windows CI and manual validation

The existing Windows matrix builds MSVC Win32/x86 and x64 with matching Python.
It tests both DLLs, inspects machine type and exact undecorated exports, checks
that FPFringe does not depend on FPAnalyzer.dll, verifies installed headers and
uploads separate FPFringe-windows-x86/x64 and FPAnalyzer-windows-x86/x64 packages.
Windows results must be read from the workflow associated with the pushed code;
local Linux validation does not establish MSVC results. Windows measured timings
are recorded in fpfringe_benchmark.csv inside each test-report artifact and in
its Actions job summary. No Windows numeric timings are substituted here.

None of Image0803.bmp, Image0792.bmp, Image0699.bmp, Image0855.bmp or Image0020.bmp
is available in the workspace. No experimental result, including the earlier
Image0020 auto-ROI failure, was fabricated or declared fixed on that actual image.
examples/fpfringe_validate.py provides the exact optional command and explicit
same-pixel old/new comparison when those files become available.

No actual NI Wizard import, new LabVIEW VI execution, Windows 11 camera driver
integration or sustained acquisition loop was available. On the user's PC,
verify x86 loading in 32-bit LabVIEW, all five import/CLFN signatures, I32 scalar
pointer for screening, explicit 4/2/4/16/64 output allocations, camera-applied
ROI increments and full-frame coordinate mapping. Keep tracking ROI fixed.
See [the complete API and manual CLFN guide](fpfringe.md).

## Source/build inventory

New files:

- include/FPFringe.h; src/FPFringe.cpp, src/FPFringe.def, src/FPFringe.map.
- src/FringeDetection.cpp and src/FringeDetection.h.
- tests/test_fpfringe.cpp, tests/test_fpfringe_c.c,
  tests/check_fpfringe_header.cmake, tests/benchmark_fpfringe.cpp.
- examples/fpfringe_ctypes_test.py, examples/fpfringe_validate.py.
- docs/fpfringe.md, this report, and the FPFringe benchmark CSV/accuracy log.

Modified files:

- src/Analysis.cpp/.h, src/Methods.cpp/.h, src/Asymmetry.cpp/.h: private
  width/height overloads and shared direct-asymmetry entry point.
- CMakeLists.txt and .github/workflows/windows-build.yml: dual libraries,
  tests, export/dependency validation and separate packaging.
- tests/check_public_header.cmake: allow the two independent public headers
  in the combined install while preserving the legacy prototype checks.
- README.md and AGENTS.md: coexistence, geometry and development guidance.

FPAnalyzer.h, FPAnalyzer.cpp, Pipeline.cpp/.h and legacy .def/.map are untouched.
