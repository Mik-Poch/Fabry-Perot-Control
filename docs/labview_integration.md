# LabVIEW integration: FPAnalyzer API v1

Primary target: **32-bit LabVIEW on Windows 11 64-bit**, using
**FPAnalyzer.dll built with MSVC 2022 Win32 (x86)**. A 32-bit process requires
a 32-bit DLL even when Windows itself is 64-bit. The separate x64 DLL is for
64-bit LabVIEW; the two DLLs have the same API and filename but cannot be
interchanged. The interface is designed for LabVIEW, but execution inside
LabVIEW has **not** been tested. Verify it with your LabVIEW version and camera
pipeline before instrument use.

## Select the DLL and runtime

On the repository's **Actions** tab, select **Windows x86 and x64 DLL**, open a
successful run for the desired U16-capable commit, and download **FPAnalyzer-windows-x86**
under **Artifacts** for 32-bit LabVIEW. Extract `bin/FPAnalyzer.dll` and
`include/FPAnalyzer.h` and `include/FPAnalyzer_LabVIEW.h`. GitHub sign-in may be required. For 64-bit LabVIEW use
**FPAnalyzer-windows-x64** instead. Check LabVIEW's bitness in **Help → About**;
the Windows architecture alone does not determine which DLL to use.

Release builds use the Microsoft Visual C++ runtime. Install the supported
**Visual C++ 2015–2022 Redistributable (x86)** for the x86 DLL, including on
64-bit Windows. An installed x64 Redistributable does not supply the x86
runtime. The x64 DLL requires the x64 Redistributable. Obtain installers from
Microsoft; do not download individual runtime DLLs from third-party sites.

## Import Shared Library Wizard

1. Download the matching successful Windows Actions artifact or build Release locally.
   Keep the DLL and both headers in a stable project directory; use an
   absolute DLL path while diagnosing loading problems. Deploy the matching
   MSVC Redistributable if the machine lacks the required runtime.
2. In LabVIEW open **Tools → Import → Shared Library** (menu wording can vary),
   choose to create VIs for a shared library, select the DLL and the public
   simplified header **`include/FPAnalyzer_LabVIEW.h`**, then select
   `FP_GetApiVersion` and `FP_AnalyzeFrameU16`. This header uses simple C
   declarations without export macros or C++ constructs. It intentionally omits
   the legacy U8 function so the image arguments are recognized as U16 arrays.
3. Set **C / cdecl** explicitly; the simplified header documents the convention
   in a comment. It is a Wizard/C declaration file, not a replacement for the
   standard C/C++ development header `FPAnalyzer.h`. Check each generated
   prototype and array setting instead of assuming the Wizard inferred them.
4. Finish the Wizard and open each generated Call Library Function Node's
   configuration. **Manual configuration of array-pointer parameters is often
   necessary.** C's `roi_out[3]` and `metrics_out[4]` declarations are pointers at
   the ABI; the Wizard may not infer their fixed allocation lengths.

## Manual Call Library Function Node configuration

Set the library path to the x86 DLL for 32-bit LabVIEW (x64 for 64-bit LabVIEW),
select the exact undecorated function name,
and choose **C / cdecl**, not stdcall. All return values are **Signed 32-bit
Integer (I32)**, passed by value. API version takes no arguments and returns 1.

For **`FP_AnalyzeFrameU16`** (recommended), configure arguments in this exact order:

| # | C argument | LabVIEW type | Passing / direction |
| --- | --- | --- | --- |
| 1 | `const unsigned short image_pixels[]` | 1D array of U16 | Array Data Pointer, input |
| 2 | `int image_width` | I32 | value, input |
| 3 | `int image_height` | I32 | value, input |
| 4 | `unsigned short cropped_pixels[]` | 1D array of U16 | Array Data Pointer, input/output buffer |
| 5 | `int cropped_capacity_pixels` | I32 | value, input |
| 6 | `int roi_out[3]` | 1D array of I32 | Array Data Pointer, input/output, length >=3 |
| 7 | `double metrics_out[4]` | 1D array of DBL | Array Data Pointer, input/output, length >=4 |

Use **Array Data Pointer**, not Array Handle or Array Handle Pointer. The DLL
expects direct element storage, not LabVIEW memory-manager handles. Do not use
a string for the grayscale image. `unsigned short` is U16, `int` is I32 even on
x64, and `double` is DBL. No argument is a pointer-to-pointer. Wire allocated
arrays into the node and take the modified arrays from the output terminals.
If your node editor provides minimum array-size expressions, set ROI to 3 and
metrics to 4, image to width×height and crop to the declared capacity; still
allocate the arrays explicitly before the call.

The library has no global mutable analysis state, so independent calls with
separate buffers may run in any thread. Use the UI thread initially while
diagnosing integration, then choose “Run in any thread” if appropriate. Keep
each input frame stable for the full duration of its call.

## Prepare input and output buffers

1. Obtain a **grayscale U16** camera image. If acquisition uses NI Vision/IMAQ,
   extract the image to an array with the appropriate Vision VI; an IMAQ image
   reference is not a U16 pixel pointer. Remove any camera row stride or padding.
   Unpack 10-/12-/14-bit packed camera formats to one U16 element per pixel;
   do not pass packed bytes or signed-I16 storage as if they were U16 grayscale.
   U16 can hold 8-, 10-, 12-, 14- or 16-bit effective data without losing camera
   precision; no intensity rescaling to 255 or 65535 is required. For an existing
   U8 camera frame, convert its elements numerically to U16, preserving values.
   Convert color images to grayscale first.
2. Flatten into a 1D row-major array, with element `y*image_width+x` equal to
   pixel `(x,y)`. For a LabVIEW 2D image, the desired array has height rows and
   width columns; confirm the mapping with an asymmetric known image to catch
   a transpose. Coordinates start at zero, X increases right, Y down.
3. Supply width and height as positive I32 values. Validate their product using
   I64 arithmetic before conversion: it must not exceed 2,147,483,647 and the
   input array length must be at least that product. Do not use pixel count in
   place of width or height.
4. Use **Initialize Array** to create a separate U16 crop array of
   `min(width,height)^2` elements. `width*height` is also sufficient. Supply its
   actual **number of U16 elements** as I32 `cropped_capacity_pixels`, never
   its byte length. For a 1280×960 input, allocate at least 960×960 = **921600
   U16 elements** for the crop (1843200 bytes). Avoid overflow in allocation
   calculations; use I64 for `width*height` and `2*capacity`. Do not alias input
   and output storage. A 32-bit process may run out of memory well below the
   API's I32 pixel-count bound.
5. Initialize a separate **I32 array of 3 elements**, e.g. `[0,0,0]`, and a
   separate **DBL array of 4 elements**, preferably filled with NaN. Empty
   arrays do not reserve output storage; do not pass them.

LabVIEW owns every array. The DLL only writes into supplied storage and never
asks LabVIEW to free native memory. Pointer lengths cannot be checked by the
DLL, so correct allocation is a caller responsibility.

## Read the outputs

Branch on the I32 return status **before** interpreting measurements:

| Status | Required handling |
| --- | --- |
| `0` | ROI, first `size²` crop U16 elements and all metrics are valid. |
| `-1` | Invalid arguments: discard results, fix dimensions, pointer configuration or allocation. For valid non-overlapping output buffers, ROI is zero and metrics NaN. On overlap rejection outputs are untouched. |
| `-2` | ROI is valid, metrics NaN, crop untouched. Allocate at least `roi[2]²` U16 elements and repeat on the **same frame**. |
| `-3` | No fringe region: ROI zero, metrics NaN, crop untouched. Skip this frame. |
| `-4` | Insufficient reliable rings/center/linewidth or resource failure: all metrics NaN. If `roi[2]>0`, the ROI and crop are valid for display/diagnostics; if zero, discard the crop. |

For success (or diagnostic crop on `-4` with size >0):

1. Index `roi_out`: `[left_x, top_y, square_size]` in **camera** coordinates.
2. Let `size=roi_out[2]`. Use **Array Subset** at index 0 with length `size*size`
   to take only valid crop U16 elements. The buffer after this range is untouched and
   is not image data, even if it contains previous-frame values.
3. Use **Reshape Array** with dimensions `size,size`. The element at row y,
   column x is `cropped_pixels[y*size+x]`. Display through an intensity graph or
   your image-display conversion VI with the expected U16 scale. The resulting
   2D array has `size` rows and `size` columns of U16, preserving every original
   pixel exactly; change a display's scale separately if needed, not this data.
4. On status 0, index metrics: `[ring_phase, ring_center_x, ring_center_y,
   finesse_proxy]`. Center coordinates are floating-point **within the crop**;
   global center is `(left_x+ring_center_x, top_y+ring_center_y)`.

Phase is normalized to [0,1). With `q=(x-cx)^2+(y-cy)^2`, bright peaks obey
`q/spacing + ring_phase = integer`. It is a fractional cavity-phase observable,
not calibrated absolute separation. A wrap from 0.99 to 0.01 may be a small
phase change; this version does not unwrap phase across frames.

Finesse proxy is squared-radius peak spacing divided by median squared-radius
FWHM. Half maximum is relative to the mean adjacent-trough baseline. It measures
effective image sharpness, not guaranteed intrinsic optical finesse.

## Diagnose loading and calls

- First create a node for `FP_GetApiVersion`: C / cdecl, no parameters, I32
  return by value. Run it and expect **1** in both architectures. API version
  stays 1 because U16 is an additive entry point; existing U8 callers keep
  their original signatures, numerical behavior and version result. Version 1
  alone does not identify a U16-capable build: also check that the DLL exports
  `FP_AnalyzeFrameU16` or use an artifact from a U16-capable commit.
- Next test `FP_AnalyzeFrameU16` with a 32×32 all-zero U16 input, a separate
  1024-element U16 crop filled with a known sentinel (e.g. 42330), ROI I32[3] and metrics
  DBL[4]. Supply width=32, height=32, capacity=1024. Expect status **-3**, zero
  ROI, NaN metrics and an untouched crop. This tests the array configuration
  without requiring detectable rings.
- Then use a known synthetic ring frame before testing camera images. On
  success check ROI bounds, compare each cropped pixel to
  `input[(y+top)*width+x+left]`, and display only the first `size*size` crop
  elements. `examples/python_ctypes_test.py` supplies an independent cdecl
  loader and synthetic numerical/crop check on the same DLL. Run it with
  **x86 Python for the x86 DLL**, or x64 Python for the x64 DLL. Python is not
  required by the DLL or LabVIEW application.
- A missing dependency or bitness mismatch can look like “DLL not found” even
  when the DLL path is correct. In a Visual Studio Developer Command Prompt,
  run `dumpbin /headers FPAnalyzer.dll`: machine **14C (x86)** must match 32-bit
  LabVIEW; **8664 (x64)** must match 64-bit LabVIEW. Windows error 193 commonly
  indicates a wrong-architecture binary. Run `dumpbin /dependents FPAnalyzer.dll`
  to list runtime imports (such as VCRUNTIME140.dll and MSVCP140.dll), then
  install or repair the matching Microsoft Redistributable if dependencies
  are missing. Error 126 can mean a dependency is missing rather than the
  analyzer DLL itself. No Python installation is needed on the instrument PC.
- `dumpbin /exports` should list exactly `FP_GetApiVersion`, `FP_AnalyzeFrameU8`
  and `FP_AnalyzeFrameU16`. Decorated or additional C++ entry points indicate the
  wrong build or configuration.
- A 32-bit process has a smaller address space. Keep frames and buffers within
  available process memory; the signed-I32 pixel-count limit is an API bound,
  not a promise that such a large frame can be allocated. Native allocation
  failures remain contained as status -4 with the existing validity rules.
- An access violation usually indicates wrong pointer/handle mode, undersized
  arrays, incorrect element type, or a wrong function prototype. The DLL cannot
  protect against invalid addresses or allocations smaller than declared.
- Blank, severely clipped or unresolved rings intentionally return an error.
  Never pass NaN metrics to control logic. Actuator control is outside v0.1.

## Intensity processing and U8 compatibility

U16 input is processed directly through a typed, double-precision intensity
view; there is no U8 conversion or reinterpretation of U16 as bytes. A histogram
finds the frame's 1st and 99th intensity percentiles. Their span is mapped to
200 floating-point analysis units, with subtraction of the lower percentile.
When the span is zero, the full range is used to retain sparse patterns. A
constant image has no contrast and returns -3. Samples are neither clipped nor
rounded by this analysis view; values outside the percentiles remain available.
The existing contrast, gradient and profile thresholds thus refer to relative
frame contrast rather than a camera's ADC range. The crop always copies the
unmodified U16 samples. Numerical output definitions above remain unchanged.

The U8 function remains available through `FPAnalyzer.h`, with its original
thresholds and behavior. It uses U8 buffers and capacities in U8 elements.
For equivalent synthetic intensities represented as U8 or scaled U16, automated
tests require global centers to agree within 0.1 pixel, circular phase within
0.005 cycle and finesse proxy within 5%. These comparisons are distinct from
the ground-truth accuracy limits of 0.7 pixel and 0.025 cycle for the noiseless
test frames. They are synthetic test tolerances, not camera calibration bounds.
