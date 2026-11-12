# Development guidance

This repository implements a single-frame Fabry–Pérot analyzer. Keep v0.1 free
of controllers, actuator commands, calibration, and frame-history state.

Use the existing checkout; each cloud task is isolated. Do not create a Git
worktree unless explicitly requested. The production target is MSVC C++17,
Windows x86 (32-bit LabVIEW) and x64; Linux builds are useful for algorithm and
sanitizer validation.

Keep the public header C-compatible and export only FP_GetApiVersion,
FP_AnalyzeFrameU8 and FP_AnalyzeFrameU16. Never allow exceptions, ownership transfer, or STL types
across the API. Preserve status and output-validity contracts when editing.
All per-frame state must remain local to the call.

Permanent distribution convention: the public files are exactly
`FPAnalyzer.dll` and `FPAnalyzer.h`. Maintain one simplified, plain C header for
both native callers and LabVIEW's Import Shared Library Wizard. Do not create
or distribute a separate LabVIEW header. Do not put compiler attributes,
export/calling-convention macros, platform conditionals or C++ linkage blocks
in the public header; it must import without special preprocessor definitions.
Use unsigned short for U16 arrays, int for I32 and double for DBL. Preserve the
legacy U8 declarations and status constants. C++ sources wrap the header include
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
