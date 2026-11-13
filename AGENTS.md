# Development guidance

This repository implements a single-frame Fabry–Pérot analyzer. Keep the analyzer free
of controllers, actuator commands, calibration, and frame-history state.

Use the existing checkout; each cloud task is isolated. Do not create a Git
worktree unless explicitly requested. The production target is MSVC C++17,
Windows x86 (32-bit LabVIEW) and x64; Linux builds are useful for algorithm and
sanitizer validation.

Keep the public header C-compatible and export only FP_GetApiVersion and
FP_AnalyzeFrameU16 and FP_AnalyzeFrameU16WithMethod. Never allow exceptions, ownership transfer, or STL types
across the API. Preserve status and output-validity contracts when editing.
All per-frame state must remain local to the call.

Permanent distribution convention: the public files are exactly
`FPAnalyzer.dll` and `FPAnalyzer.h`. Maintain one simplified, plain C header for
both native callers and LabVIEW's Import Shared Library Wizard. Do not create
or distribute a separate LabVIEW header. Do not put compiler attributes,
export/calling-convention macros, platform conditionals or C++ linkage blocks
in the public header; it must import without special preprocessor definitions.
Use unsigned short for U16 arrays, int for I32 and double for DBL. Preserve
status constants and API version 2. C++ sources wrap the header include
in extern "C" in their own source. MSVC uses /Gd; compiler definitions for
exports/cdecl are PRIVATE to the DLL target and Windows names come from the
existing src/FPAnalyzer.def. Never move them into the public header.

When adding a public function, update FPAnalyzer.h, its implementation, Windows
.def and Linux export map together. Extend the signature/ABI tests and the
public-header contract test's required function list. Verify that dumpbin
exports exactly the names declared by the header on x86 and x64. Every Windows
DLL artifact must include FPAnalyzer.dll and the identical FPAnalyzer.h and no
second public header. Keep the package validation step before artifact upload.

Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, then
`cmake --build build --config Release --parallel 4` and
`ctest --test-dir build -C Release --output-on-failure`.
On Windows configure with `-G "Visual Studio 17 2022" -A Win32` for x86 or
`-A x64` for x64, using separate build directories and matching Python bitness.
Use a separate `build-asan` with `-DFP_ENABLE_SANITIZERS=ON` on Linux.
Tests must run in Release too; do not use disabled `assert` for test checks.

Record accuracy, failure statuses and measured timing; never claim Windows or
LabVIEW validation from a Linux test alone. Real camera frames are optional
local inputs under data/test_frames and must not be committed without consent.
Header compilation and ABI calls do not establish Wizard import, automatic VI
generation or actual LabVIEW execution; report those outcomes separately.

API v2 is U16-only. FP_AnalyzeFrameU16 has roi_in[3] before the crop buffer;
never restore U8 exports or silently change the ABI without a version bump.
[0,0,0] selects auto detection. Other ROIs require positive size and exact
in-bounds cropping; invalid ROIs are errors, never clamped. Manual ROI must
bypass all full-frame detection, downsampling and intensity statistics. Both
modes normalize/analyze the crop and return crop-relative centers with the
same phase/finesse conventions. Preserve original U16 samples in the output.

Keep the distinction between the low-level caller-buffer API and the four-input
FP Analyze Frame U16.vi wrapper. The wrapper manages capacity internally,
normalizes empty ROI to an allocated I32[3], explicitly allocates ROI I32[3]
and metrics DBL[4], and trims valid crops. C array notation does not configure
LabVIEW allocation. Never resize LabVIEW Array Data Pointers with ordinary
C/C++ allocators; do not add LabVIEW Memory Manager integration for this API.
Benchmark automatic and manual ROI separately using equivalent crops.

Feature generation v3 is additive to ABI v2: FP_GetApiVersion still returns 2
for existing equality checks. Discover selector support by resolving
FP_AnalyzeFrameU16WithMethod; do not infer it from version 2 alone. The original
FP_AnalyzeFrameU16 must delegate method 1 with its exact signature, defaults,
ROI handling and existing status values. Invalid selector is -5, not -1.
Methods are 0=A (opposite spokes), 1=B (radial reference), 2=C (2D Airy),
3=D (eight sectors). Never silently fall back to B if a selected method fails.
B initialization for C/D is allowed, but successful C/D outputs must come from
the selected fit. Keep A independent of full-image gradient-center fitting.

Keep diagnostics, scratch data and timing per call; no global or thread_local
mutable history. Maintain concurrent-call tests with disjoint output buffers.
The common pipeline in Pipeline.cpp owns validation/ROI/copy/normalization;
Methods.cpp owns algorithm dispatch and fit-specific checks. Phase follows
q/spacing+phase=integer; centers are crop-relative. C's finesse uses the
trough-relative Airy FWHM pi/(2*asin(1/sqrt(F+2))); it is not intrinsic finesse.
Sector residuals are dimensionless diagnostics, not mechanical tilt angles.
The old LabVIEW wrapper remains four inputs; the selector wrapper adds only an
I32 method input, with crop capacity internal in both. Never grow metadata
outputs to compensate for a generated-VI allocation bug (6/8 or other lengths).

Run methods regression/accuracy/concurrency and matching-bitness ctypes tests.
Golden B measurements come from a9a0316 with tolerances documented in the test.
fp_methods_benchmark runs warm-ups and reports median/p95/min/max, failures and
per-stage timings on identical ROIs. The Release CSV must accompany the CI test
report. fp_b_compare accepts a same-architecture previous DLL/library for
interleaved before/after benchmarks; do not compare different computers as if
an optimization caused their timing difference. Reject/record unsuitable
patterns and publish synthetic limitations and optional real-frame skips.
