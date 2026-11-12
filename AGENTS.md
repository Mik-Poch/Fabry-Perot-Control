# Development guidance

This repository implements a single-frame Fabry–Pérot analyzer. Keep v0.1 free
of controllers, actuator commands, calibration, and frame-history state.

Use the existing checkout; each cloud task is isolated. Do not create a Git
worktree unless explicitly requested. The production target is MSVC C++17,
Windows x86 (32-bit LabVIEW) and x64; Linux builds are useful for algorithm and
sanitizer validation.

Keep the public header C-compatible and export only FP_GetApiVersion and
FP_AnalyzeFrameU8. Never allow exceptions, ownership transfer, or STL types
across the API. Preserve status and output-validity contracts when editing.
All per-frame state must remain local to the call.

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
