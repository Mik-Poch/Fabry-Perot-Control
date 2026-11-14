# FPFringe validation and performance, 2026-11-14

## Verified on Linux x86_64

GCC 14.2.0, C++17, Release; Python 3.12 ctypes. The full Release CTest run
passed 18 runnable entries with three optional experimental-image entries
explicitly skipped. Existing legacy analysis, U16/selector regression, concurrency,
public C/C++ header and ctypes tests passed without relaxed tolerances. Dedicated
FPFringe API 2 tests exercise four C exports, plain C99 function-pointer signatures,
C/cdecl calls, output canaries, deterministic failure outputs, nulls, overflow,
invalid method IDs, overlap rejection and concurrent independent buffers across
all three processing functions. The old public header and export definitions
are unchanged.

ASan+UBSan RelWithDebInfo: all 12 runnable entries passed, one optional image
entry skipped, no sanitizer findings. The affected FPFringe targets were rebuilt
and rechecked after the final rectangular/angular/ROI fixture additions.
The installed-package checks passed for both public headers. ELF inspection
found exactly four FPF_ exports in FPFringe and the same four FP_ exports in
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

## API 2 cleanup: direct measurement validity

The standalone checker export, declaration, implementation and unused private
wrapper have been removed. FPF_GetApiVersion now returns 2, independently of
the unchanged legacy FPAnalyzer API version 2. The four retained signatures are
unchanged; no replacement checking stage or analyzable output was introduced.

The private evidence() and findCenteredRoi() bodies were compared with the
pre-cleanup source and are unchanged. The retained exported ROI, analysis and
asymmetry function bodies are also unchanged. Numerical Analysis/Methods/Asymmetry
core files and all legacy source, headers, exports and tests were untouched.

A same-toolchain FPFringe API-1 binary was retained before cleanup. Comparing
its retained calls against API 2 gave **882 exact output comparisons and 70
identical statuses**: ROI geometry/subpixel centers for automatic, exact and
invalid sizes; A/B/C/D metrics and all 16/64 asymmetry fields; 8/10/12/14/16-bit
square inputs and landscape/portrait images. Paired NaNs and failed statuses
were compared explicitly. No changed numerical outputs were observed.

The native suite retains all ROI/geometry/rectangle/phase/finesse/asymmetry,
canary, invalid-argument, overlap and concurrent-call checks. It additionally
verifies a two-ring ROI still succeeds while strict method B returns -4 with
NaN metrics. Blank images return -4 with NaN metrics for A/B/C/D and invalid
[NaN,NaN,0,0] sectors for asymmetry. No numerical tolerances were weakened.
The ctypes suite confirms the removed symbol cannot be resolved and still
performs 480 exact old-FPAnalyzer/new-FPFringe comparisons.

Continuous LabVIEW measurement calls analysis directly, accepts status 0 subject
to application quality checks, and discards nonzero/NaN results. Ring/sector
counts and harmonic RMS remain additional asymmetry quality indicators. Handling
consecutive failures, reacquisition and any piezo response belong to LabVIEW.
No regulator logic or extra validity output is implemented in the DLL.

## Measured Release timings

[Full CSV](benchmarks/2026-11-14-linux-fpfringe.csv), 64-bit Linux, two warm-ups
and 21 measured end-to-end C calls per row, rendering excluded. Median is index
10; nearest-rank p95 is index 19. All 378 measured calls succeeded, no failures.
These are measured shared-cloud CPU latencies, not fabricated Windows timings
or a real-time guarantee. Input parameters are in tests/benchmark_fpfringe.cpp.

| Image | Operation | Median ms | p95 ms | Success/failure |
|---|---|---|---|---|
| 320x320 | ROI | 5.094 | 5.821 | 21/0 |
| 320x320 | A | 1.120 | 1.202 | 21/0 |
| 320x320 | B | 4.700 | 5.154 | 21/0 |
| 320x320 | C | 20.331 | 21.390 | 21/0 |
| 320x320 | D | 6.620 | 7.213 | 21/0 |
| 320x320 | asymmetry | 5.525 | 6.321 | 21/0 |
| 640x480 | ROI | 12.305 | 16.218 | 21/0 |
| 640x480 | A | 2.518 | 2.789 | 21/0 |
| 640x480 | B | 13.026 | 14.825 | 21/0 |
| 640x480 | C | 29.952 | 40.576 | 21/0 |
| 640x480 | D | 16.033 | 18.106 | 21/0 |
| 640x480 | asymmetry | 13.987 | 16.759 | 21/0 |
| 1280x960 | ROI | 41.062 | 44.535 | 21/0 |
| 1280x960 | A | 6.049 | 6.482 | 21/0 |
| 1280x960 | B | 43.898 | 48.211 | 21/0 |
| 1280x960 | C | 51.005 | 66.646 | 21/0 |
| 1280x960 | D | 54.522 | 57.519 | 21/0 |
| 1280x960 | asymmetry | 43.462 | 60.521 | 21/0 |

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
verify x86 loading in 32-bit LabVIEW, all four import/CLFN signatures and API version 2, explicit 4/2/4/16/64 output allocations, camera-applied
ROI increments and full-frame coordinate mapping. Keep tracking ROI fixed.
See [the complete API and manual CLFN guide](fpfringe.md).

## Files modified by the API 2 cleanup

- include/FPFringe.h.
- src/FPFringe.cpp, src/FPFringe.def, src/FPFringe.map.
- src/FringeDetection.cpp, src/FringeDetection.h.
- tests/test_fpfringe.cpp, tests/test_fpfringe_c.c,
  tests/check_fpfringe_header.cmake, tests/benchmark_fpfringe.cpp.
- examples/fpfringe_ctypes_test.py, examples/fpfringe_validate.py.
- .github/workflows/windows-build.yml.
- README.md, AGENTS.md, docs/fpfringe.md, this validation report.
- docs/benchmarks/2026-11-14-linux-fpfringe.csv and
  docs/benchmarks/2026-11-14-linux-fpfringe-accuracy.txt.

CMakeLists.txt, the shared numerical algorithms and all FPAnalyzer source,
headers, export definitions and legacy tests are unchanged by this cleanup.
