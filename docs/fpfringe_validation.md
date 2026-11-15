# FPFringe API 4: exact legacy method-0 ROI, 2026-11-15

## Implementation and ABI

FPF_FindCenteredRoiU16 now calls `fp::analyzeFrame` directly with automatic ROI
`{0,0,0}`, method `0` and private unused metrics[4]. It passes the caller's
image, dimensions, cropped buffer, capacity and ROI pointer without additional
validation or processing. The complete existing detection/cropping/method-0
pipeline runs; its metrics are discarded. No new ROI algorithm is introduced.

```c
int FPF_FindCenteredRoiU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    unsigned short cropped_pixels[],
    int cropped_capacity_pixels,
    int roi_out[3]);
```

FPF_GetApiVersion returns **4**, while legacy FP_GetApiVersion remains **2**.
There are exactly four undecorated FPF exports. The ROI ABI has six parameters,
without requested size or independent center output. Old generated nodes must
be reconfigured before calling the new DLL. The name does not impose any new
centering tolerance or sizing rule on the legacy result.

The unused private FringeDetection.cpp/.h module was removed from source and
fp_core. Pipeline.cpp/.h, Methods.cpp/.h, Analysis.cpp/.h, Asymmetry.cpp/.h, legacy
headers/exports and numerical thresholds are unchanged. Both retained FPF
analysis signatures and bodies are unchanged. Each DLL links the existing
static fp_core; FPFringe has no runtime dependency on FPAnalyzer.dll.

## Direct reference comparisons

Every ROI fixture calls FP_AnalyzeFrameU16WithMethod with automatic ROI and
method 0, then the new six-parameter wrapper with the same input and capacity.
The native regression checks identical status, all three ROI integers and the
complete output buffers byte-for-byte, including untouched tails. It also
checks source preservation, crop/ROI canaries, original U16 samples, disjoint
concurrent calls and identical overlap rejection.

[Recorded native comparisons](benchmarks/2026-11-15-linux-method0-roi-regression.txt)
include square 320x320, landscape 640x480, portrait 480x640 and compact
1280x960 patterns; off-center and bright-center/weak-outer fixtures; single-ring,
blank and stripe failures; zero, short and exact capacities; null arguments;
invalid/overflow dimensions; negative capacity and partial overlaps. The
1280x960 regression allocates **921600 actual U16 elements** plus guard storage.

Representative results below are synthetic regression observations, not camera
measurements. Old and new results match in every row and all crop bytes match.

| Fixture | Legacy/new status | Legacy/new ROI [left,top,side] |
|---|---|---|
| Square 320x320 | 0 / 0 | [4,4,312] |
| Landscape 640x480 | 0 / 0 | [130,50,381] |
| Portrait 480x640 | 0 / 0 | [50,130,381] |
| Compact 1280x960 | 0 / 0 | [500,308,312] |
| Same frame, capacity 0 or 97343 | -2 / -2 | [500,308,312], crop untouched |
| Same frame, exact capacity 97344 | 0 / 0 | [500,308,312] |
| Single ring 1280x960 | -4 / -4 | [570,378,173], complete diagnostic crop |
| Stripes 640x480 | -4 / -4 | [80,0,480], complete diagnostic crop |
| Blank 640x480 | -3 / -3 | [0,0,0], crop untouched |
| Null image / bad dimensions / negative capacity | -1 / -1 | [0,0,0], crop untouched |

In particular, **-4 does not imply an untouched crop**: the legacy method can
fail after copying all pixels and writing geometry. This behavior is preserved,
not converted into success. Known overlaps return -1 before output writes.
Insufficient capacity returns -2 with valid geometry and no crop-pixel writes.

## Local validation

Linux x86_64, GCC 14.2, Release: **19 passed, 3 explicitly skipped, 0 failed**
(22 selected). ASan/UBSan RelWithDebInfo: **13 passed, 1 explicitly skipped,
0 failed** (14 selected). Sanitized native ROI tests found no memory errors.
The skipped checks require optional real images; sanitizer builds intentionally
exclude Python-hosted ctypes and timing tests.

The compiled C99 ABI client validates the six-argument function pointer, API 4,
status handling and output canaries. The header contract checks the exact
prototype, four exports and installed header equality. Matching-bitness ctypes
checks pass 480 exact legacy/new analysis output comparisons, including paired
NaNs. An additional comparison against the pre-change API-3 FPFringe library
gave **672 exact analysis values and 35 identical statuses**, including NaNs,
across 8/10/12/14/16-bit and square/landscape/portrait fixtures. The old ROI ABI
was not called through the new signature.

Installed-package checks pass for both libraries. Linux nm lists exactly four
FPF exports and ldd shows no FPAnalyzer dependency. Windows uses dumpbin for
architecture, undecorated export names and dependent-DLL checks.

## Release performance

[Full measured CSV](benchmarks/2026-11-15-linux-method0-roi.csv): 20 rows,
two warm-ups and 21 measured calls per row; **420 successes, 0 failures**.
The ROI operation includes the complete legacy method-0 pipeline, including
numerical analysis with discarded metrics. Input generation and caller buffer
allocation are outside timing. Median is sorted index 10; nearest-rank p95 is
index 19. These Linux shared-host observations are not Windows measurements,
an optimization claim or a real-time guarantee.

| Frame | Pattern | Side | Median ms | p95 ms | Success/failure |
|---|---|---|---|---|---|
| 320x320 | wide | 320 | 2.234 | 2.852 | 21/0 |
| 640x480 | wide | 480 | 3.137 | 3.422 | 21/0 |
| 1280x960 | wide | 960 | 7.532 | 8.273 | 21/0 |
| 1280x960 | compact | 312 | 2.685 | 2.942 | 21/0 |
| 1280x960 | weak_outer | 346 | 2.870 | 3.064 | 21/0 |

The CSV also records the unchanged A/B/C/D/asymmetry operations. Earlier API-3
measurement files remain historical records, not the active ROI specification.
Windows runners generate their own measured CSV per architecture.

## LabVIEW crash investigation and remaining validation

A numeric capacity of 960*960 does not allocate an array. A 1280x960 caller
must actually initialize at least **921600 U16 elements (1843200 bytes)** before
passing that capacity. Derive capacity from Array Size of that allocated array.
Supply I32 ROI[3], pass both arrays as **Array Data Pointer**, select C/cdecl,
I32 value dimensions/capacity/return, and configure exactly six parameters.
Use x86 with 32-bit LabVIEW. Replace older requested-size/center-output nodes,
and check API version 4 before calling. The DLL never allocates/resizes/frees
LabVIEW-owned arrays. See the [complete CLFN table and allocation steps](fpfringe.md#labview-acquisition-and-clfn-setup).

An undersized allocation, array handle, wrong element type or old argument
layout can corrupt memory; raw pointers do not carry their allocation lengths.
The native tests verify the actual 921600-element allocation and insufficient
declared capacities safely. They deliberately do not pass a short array with
a falsely large capacity. No VI or crash dump was supplied, so the reported
LabVIEW crash's precise cause and actual runtime resolution are **unverified**.
No NI Wizard import, generated VI or LabVIEW runtime test was performed.

**Image0180.bmp was unavailable and was not tested.** None of
Image0803/0792/0699/0855/0020.bmp or other experimental BMPs were present either.
Synthetic frames do not establish success on those images. The optional
validator now compares legacy/new full-frame ROI status, geometry, complete
crop buffers, canaries and source pixels before comparing analyses on a valid
returned diagnostic crop. Negative method-0 statuses are reported and checked
for equivalence. Missing frames return exit 77 when no frame can be executed;
an actual comparison mismatch returns failure.

The Windows workflow builds and executes the compiled DLL tests on **Win32/x86
and x64**, with matching Python bitness. It verifies the exact six-parameter
API-4 header, package header hashes, architecture, four exports, C ABI and no
runtime FPAnalyzer dependency. It retains FPFringe-windows-x86/x64,
FPAnalyzer-windows-x86/x64 and both test-report artifacts with CTest logs/CSVs.
Consult the GitHub Actions run associated with the commit for actual Windows
results; Linux results alone do not establish Windows execution.

## Changed files

- include/FPFringe.h and src/FPFringe.cpp: API 4 and direct legacy wrapper.
- src/FringeDetection.cpp/.h: deleted; CMakeLists.txt: remove unused core source.
- tests/test_fpfringe_roi.cpp, test_fpfringe.cpp, test_fpfringe_c.c,
  check_fpfringe_header.cmake and benchmark_fpfringe.cpp: six-argument API and
  direct reference regression/ABI/timing checks.
- examples/fpfringe_ctypes_test.py and fpfringe_validate.py: bindings and exact
  old/new ROI comparisons, including optional experimental frames.
- .github/workflows/windows-build.yml: explicit API-4 package-check step;
  its shared checker enforces the new prototype on both architectures.
- README.md, AGENTS.md, docs/fpfringe.md, this report and the two linked
  method0-roi measurement files: current contracts, allocation and evidence.

All legacy exports, headers, algorithms and existing legacy test tolerances
remain unchanged. The PR is updated on feat/fp-analyzer-v0.1 and is not merged.
