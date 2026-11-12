# Validation record

Recorded during implementation on 2026-11-12. Local measurements below describe
the Linux development build. The Windows CI outcome is recorded separately;
no LabVIEW execution has been performed.

## Environment and commands

- Linux x86_64, glibc 2.41; reported CPU Intel Xeon Platinum 8573C (shared cloud).
- GCC/G++ 14.2.0, CMake 3.31.10, Ninja 1.13.0, Python 3.12.14.
- Normal build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`,
  `cmake --build build --parallel 4`,
  `ctest --test-dir build --output-on-failure --verbose`.
- Sanitizers: separate Debug build with `-DFP_ENABLE_SANITIZERS=ON`, same native
  tests under AddressSanitizer and UndefinedBehaviorSanitizer.
- Installation: `cmake --install build --prefix /workspace/fp-install-check`,
  then the ctypes program loaded the installed shared library successfully.

## Results

| Check | Outcome |
| --- | --- |
| Release C++ suite | 6/6 groups passed; active checks in Release |
| C99 header compilation and C caller | passed |
| Independent Python ctypes load and analysis | passed |
| ASan + UBSan native suite and C caller | passed, no sanitizer diagnostics |
| Optional real camera frames | skipped; no frames supplied |
| Linux dynamic symbol inspection | exactly the two API names exported |
| Install layout / installed-library ctypes | passed |
| Windows x64 / MSVC build, C++/C/ctypes tests and DLL exports | passed in Actions run 43308997904, source commit 25ba4a1 |
| LabVIEW Import Wizard / live Call Library Function Node | not executed |

Synthetic frames include rectangular/portrait/square images, a localized small
pattern in a 1280×960 frame, cropped edge patterns with enough complete rings,
fractional centers, phases near both sides of the wrap, +1 and negative phase,
low brightness (28 gray levels over background with sigma-2 noise), and sigma-8
noise on a brighter pattern. API checks require center error below 0.7 pixels
(1.0 for noisy cases) and circular phase error below 0.025 cycles (0.04 noisy).
All those checks passed. The analytic profile test checks phase within 0.001
cycles and trough-relative Airy FWHM within 3% of its closed-form value.

One representative noiseless 640×480 frame, truth `(cx,cy)=(319.3,239.7)`,
phase 0.27 and Airy coefficient 24, returned camera center approximately
`(319.300,239.700)`, phase `0.268412`, finesse proxy `7.94208`.
Reducing Airy coefficient to 5 (broader peaks) gave proxy `4.0331`.
These are synthetic validation examples, not instrument calibration results.

Invalid dimensions, overflow products, null pointers, overlapping arrays,
zero/one-byte-short/exact capacity, output canaries, crop equality, constant
images, a lone hot pixel, stripes, a Gaussian spot and multiple pure-noise seeds
are covered. Failed numerical measurements leave NaN metrics; insufficient
capacity never writes crop bytes.

## Measured timing

The Release C++ suite's representative 1280×960 synthetic frame uses center
`(645.4,470.8)`, radius 340 px, squared-radius period 15000 px², phase 0.27,
Airy coefficient 24 and sigma-3 noise. Image generation is outside the timer;
ROI detection, allocation, crop and measurement are inside it. The recorded
full-suite run reported **77.42 ms per call**, arithmetic mean of three
consecutive calls. Earlier runs in the same shared cloud ranged approximately
51–91 ms. This is a measured development observation, not a real-time bound or
a prediction of Windows/instrument performance. CI prints its own timing.

## Original Windows x64 validation

The Windows Actions workflow builds Release with Visual Studio 2022 x64,
executes the C++, C ABI and ctypes tests, checks `dumpbin /exports`, and uploads
the DLL/header only on success. [Run 43308997904](https://github.com/Mik-Poch/Fabry-Perot-Control/actions/runs/43308997904)
for source commit `25ba4a1` completed **successfully**. The public Actions page
confirmed its conclusion and published `FPAnalyzer-windows-x64` and
`FPAnalyzer-test-report` artifacts. Thus the compiled Windows DLL also passed
the independent Python ctypes loader/numerical test and the exact MSVC export
check. The public page requires sign-in to read detailed logs/download artifacts;
no separate Windows performance number is asserted here.

Creating the pull request through `gh pr create` was blocked by the cloud egress
proxy on `api.github.com/graphql` (Forbidden; independent API CONNECT probe 403).
Adding `api.github.com` was saved in the environment draft but has not been
applied. Both the empty initial `main` base and the implementation feature branch
were pushed successfully through normal Git authentication. This networking
limitation did not prevent viewing the successful CI outcome via github.com.
Real-camera accuracy and actual LabVIEW calls still require external validation.

## Windows x86 and x64 support

The architecture extension started from the complete implementation at
`ac429e9e90e7b72eb45b3d55f4a7fda80dd8e998` on `feat/fp-analyzer-v0.1`.
Implementation commit `2445f88ad42a4a40346c57b1a7b215d6db788ec7` removes the
64-bit-only CMake restriction and adds a Visual Studio 2022 Win32/x64 matrix.
The image-analysis sources, public header, API version 1, cdecl convention and
export definition file are unchanged.

### 32-bit portability review

- ABI assertions already require 8-bit bytes, 32-bit `int` and IEEE-754 `double`.
- The API multiplies positive image dimensions in `uint64_t`, rejecting products
  above `INT_MAX` before reading pixels or converting the count to `size_t`.
  Regression cases now include 46341×46341 (above signed-I32 range) and
  65536×65536 (wraps to zero if multiplied in unsigned 32-bit arithmetic).
- Pixel/crop offsets use `size_t`; the bounded image count and square ROI within
  the input dimensions keep these offsets representable on x86. Overlap checks
  use `uintptr_t` subtraction rather than overflowing end-address additions.
- Larger floating-point work buffers may exceed a 32-bit process's available
  memory. Standard container allocation failures are caught at the C boundary
  and mapped to the existing -4 status; the pixel-count limit does not promise
  that every theoretically valid allocation will fit.

### Executed checks

[Push run 43321809240](https://github.com/Mik-Poch/Fabry-Perot-Control/actions/runs/43321809240)
for implementation commit `2445f88` completed **successfully**, as did
[PR #1 run 43321819033](https://github.com/Mik-Poch/Fabry-Perot-Control/actions/runs/43321819033).
The public run summaries confirmed success and four uploaded artifacts per run.

| Check | Win32 / x86 | x64 |
| --- | --- | --- |
| MSVC 2022 Release DLL and test executables | passed | passed |
| Existing native C++ tests plus overflow regressions | passed | passed |
| C99 header / C ABI caller | passed | passed |
| Python 3.12 ctypes with matching interpreter architecture | passed (x86) | passed (x64) |
| `dumpbin /headers` machine check | passed (14C) | passed (8664) |
| `dumpbin /exports` exact undecorated API names | passed | passed |
| DLL/header artifact uploaded | `FPAnalyzer-windows-x86` | `FPAnalyzer-windows-x64` |
| Test-log artifact uploaded | `FPAnalyzer-test-report-x86` | `FPAnalyzer-test-report-x64` |

Both DLLs export exactly `FP_GetApiVersion` and `FP_AnalyzeFrameU8`; no changes
to `src/FPAnalyzer.def` were necessary for MSVC x86 name decoration. Each
distributable contains `bin/FPAnalyzer.dll` and `include/FPAnalyzer.h`.
GitHub sign-in is required to download artifacts and inspect detailed logs.
The cloud proxy still denies GitHub API access; confirmation here uses the
public github.com run summaries and successful execution of the committed
workflow, not an API response or a local Windows build.

The local Linux x86_64 Release build also passed all 6 C++ groups, the C ABI
caller and ctypes test (3 tests passed). Its timing case reported 68.28 ms per
call, the mean of 3 calls; this is not a Windows timing measurement. A separate
Debug ASan/UBSan build passed the native suite and C caller without sanitizer
diagnostics (2 tests passed; ctypes is excluded by design). The optional
real-camera test was skipped in each local run because no frames were supplied;
the repository contains no real camera frames for CI either.

Actual 32-bit LabVIEW calls on Windows 11 and camera/instrument validation
remain unexecuted. See `docs/labview_integration.md` for artifact selection,
runtime dependencies and initial Call Library Function Node checks.
