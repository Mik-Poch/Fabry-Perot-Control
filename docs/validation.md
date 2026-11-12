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

## Windows CI and remaining external validation

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
