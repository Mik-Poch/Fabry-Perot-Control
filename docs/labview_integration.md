# LabVIEW U16 integration — feature v3, compatible ABI v2

Use **FPAnalyzer.dll** with **FPAnalyzer.h**. These permanent filenames and the
single simplified plain C header must be preserved. The user reports the existing U16 function working in 32-bit LabVIEW on Windows
11 64-bit and successful Wizard import of this simplified header format. This
release preserves that function and adds a selector. The new node is checked by
compiled C/C++ ABI and ctypes tests; its actual Wizard wiring/execution has not
been performed here. No `.vi` file was generated or automatically corrected.

Use the **x86/Win32 DLL for 32-bit LabVIEW**, including on 64-bit Windows 11;
use x64 only for 64-bit LabVIEW. Install the matching Microsoft VC++ 2015–2022
Redistributable. Never use the old v1 generated node with v2: one argument was
inserted and U8 was removed. Reimport or edit the complete prototype first.

## New selectable-method wrapper (five inputs)

Keep **FP Analyze Frame U16.vi** and its four-input connector pane unchanged.
Create a separate **FP Analyze Frame U16 With Method.vi** with the same image,
W, H and ROI inputs plus **analysis_method (I32)**. Outputs stay crop U16[1D],
ROI I32[3], metrics DBL[4] and I32 status. Capacity/allocation remain internal.
Use enum/ring items 0=A opposite spokes, 1=B radial reference, 2=C 2D Airy,
3=D angular sectors; default **1** preserves the reference behavior.

Configure its Call Library Function Node for
`FP_AnalyzeFrameU16WithMethod`, C/cdecl, I32 return by value:

| # | Parameter | Node setting |
| --- | --- | --- |
| 1 | image_pixels | 1D U16 Array Data Pointer; at least W*H elements |
| 2 | image_width | I32, Value |
| 3 | image_height | I32, Value |
| 4 | roi_in | 1D I32 Array Data Pointer; minimum size 3 |
| 5 | analysis_method | I32, Value (not pointer or array) |
| 6 | cropped_pixels | 1D U16 Array Data Pointer; internally initialized capacity |
| 7 | cropped_capacity_pixels | I32, Value; internal U16 element count |
| 8 | roi_out | 1D I32 Array Data Pointer; minimum size constant 3 |
| 9 | metrics_out | 1D DBL Array Data Pointer; minimum size constant 4 |

Use exactly the allocation, ROI normalization, trimming and error branches
below; only the selector and parameter numbers differ. `FP_GetApiVersion()`
continues returning **2**, including for existing equality checks. Discover new
selector support by looking up `FP_AnalyzeFrameU16WithMethod`; the ABI version
alone cannot distinguish older v2 DLLs. Never change the original node's
prototype to insert a selector: choose the new function name on the new node.
Invalid selectors return **-5**, zero ROI, four NaNs and untouched crop. A valid
selected method which cannot measure reliably returns -4, with no fallback.

## Correcting the reported 6/8-element result arrays

The DLL writes **exactly three I32 values and four DBL values**. The reported
6-element ROI and 8-element metrics point to LabVIEW allocation/wiring (for
example preallocated lengths or concatenated results), not extra native output.
Without the actual VI we cannot identify which wire caused the doubling.
Do not make the DLL write 6/8, or alter the C element types to compensate.

Probe Array Size immediately after each Initialize Array, immediately after
the node, and at the wrapper output. This shows where an oversized array is
introduced. Initialize separate **I32[3]** and **DBL[4]** buffers on every call;
configure minimum sizes as constants **3** and **4**; remove shared crop/image
size expressions and any Build Array/Concatenate combining old and new results.
Minimum size is a lower bound: it does **not shrink** an incoming 6/8 array.
Use direct result wires to the indicators (or an explicit Array Subset index 0,
length 3/4 at a compatibility-wrapper boundary). Confirm exact sizes on every
success/error path. No C notation or import alone proves that the VI is fixed.

## Public original-wrapper connector pane

Create **FP Analyze Frame U16.vi** with exactly these terminals:

| Direction | Name | LabVIEW type |
| --- | --- | --- |
| input | image_pixels | 1D U16 array |
| input | image_width | I32 |
| input | image_height | I32 |
| input | roi_in | 1D I32 array, 3 elements; empty means auto at wrapper level |
| output | cropped_pixels | 1D U16 array, exactly size² valid elements, or empty on error without crop |
| output | roi_out | 1D I32 array, exactly 3 elements |
| output | metrics_out | 1D DBL array, exactly 4 elements |
| output | status | I32 |

Do not add a capacity or preallocated-output input to this connector pane.
The private generated binding/Call Library Function Node has more arguments;
those buffers are implementation details inside the wrapper.

## Low-level Call Library Function Node

Import `FPAnalyzer.h` using the Import Shared Library Wizard or configure a
Call Library Function Node manually. Choose C / **cdecl**, function
`FP_AnalyzeFrameU16`, return type **Signed 32-bit Integer by value**. Verify all
eight parameters in this exact order; fixed-size notation in C does not tell
the Wizard how much LabVIEW memory to allocate.

```c
int FP_AnalyzeFrameU16(
    const unsigned short image_pixels[],
    int image_width,
    int image_height,
    const int roi_in[3],
    unsigned short cropped_pixels[],
    int cropped_capacity_pixels,
    int roi_out[3],
    double metrics_out[4]);
```

| # | Parameter | Node configuration | Minimum size / source |
| --- | --- | --- | --- |
| 1 | image_pixels | Array, Unsigned 16-bit Integer, 1 dimension, Array Data Pointer | W*H elements, validated before call |
| 2 | image_width | Signed 32-bit Integer, Value | W |
| 3 | image_height | Signed 32-bit Integer, Value | H |
| 4 | roi_in | Array, Signed 32-bit Integer, 1 dimension, Array Data Pointer | 3; normalized allocated input |
| 5 | cropped_pixels | Array, Unsigned 16-bit Integer, 1 dimension, Array Data Pointer | cropped_capacity_pixels; initialized buffer |
| 6 | cropped_capacity_pixels | Signed 32-bit Integer, Value | internal capacity in U16 elements, not bytes |
| 7 | roi_out | Array, Signed 32-bit Integer, 1 dimension, Array Data Pointer | constant 3; separately initialized I32[3] |
| 8 | metrics_out | Array, Double-precision Float, 1 dimension, Array Data Pointer | constant 4; separately initialized DBL[4] |

For crop minimum size, select the capacity parameter if the LabVIEW version
supports parameter-based size expressions; independently wire an explicitly
initialized buffer of that length. Do not use string, handle, Array Handle,
pointer-to-pointer, Adapt to Type, U8 or signed I16 for the U16 buffers. Do not
configure input/output ROI to share storage. Enable constant/input protection
for read-only arrays if that option is available in the installed version.
The implementation is stateless and reentrant; Run in any thread is supported.

The DLL writes only 3 ROI values and 4 metric values. It cannot shrink an array
that the Wizard allocated to 8194 elements. Correct that allocation in LabVIEW.

## Exact wiring and correction of a generated VI

Keep a low-level generated VI private, or place the configured node directly
inside the user-facing wrapper. Wire the following Case Structure flow:

1. **Initialize failure outputs**: empty U16 `cropped_pixels`, Initialize Array
   with an I32 zero and dimension size **3** for `roi_out`, and Initialize Array
   with a DBL NaN and dimension size **4** for `metrics_out`. Use explicitly
   typed constants so no default I32/DBL coercion changes the array types.
2. **Validate dimensions before multiplication or allocation.** Require W>0,
   H>0. Convert both to I64 *before* multiplication. Compute W*H and
   `capacity64 = I64(min(W,H))*I64(min(W,H))`. Require both counts <=2147483647.
   Also check U16 byte counts in I64 and available process memory, especially
   for LabVIEW 32-bit; satisfying the I32 count bound does not guarantee an
   allocation will succeed. Require Array Size(image_pixels)=W*H. On failure,
   bypass the DLL, return status -1 and the initialized failure outputs.
3. **Normalize roi_in** using Array Size and a Case Structure: size 0 selects
   an allocated I32 array constant `[0,0,0]`; size 3 passes the supplied array;
   any other length bypasses the DLL with status -1. A zero-length C pointer
   must never reach the node. C cannot determine how many elements exist.
4. **Allocate crop internally.** Convert checked capacity64 to I32, then
   Initialize Array with U16 zero and that dimension size. Wire its output to
   cropped_pixels (parameter 5) and capacity to parameter 6. Handle a LabVIEW
   allocation failure on the error path without calling the DLL; return -4,
   empty crop, zero ROI and NaN metrics. A 1280×960 input needs a worst-case
   960²=921600-element buffer (1843200 bytes). Do not put capacity on the pane.
5. **Replace oversized generated outputs.** Delete the Wizard's ~8194-element
   ROI/metrics allocation constants or size expressions. Wire the dedicated
   Initialize Array outputs of lengths **3** and **4** to parameters 7 and 8.
   In Configure → Parameters set their Minimum size to constant **3** and **4**.
   Ensure no shared size parameter, image size, crop capacity or Wizard default
   drives these two arrays. Remove output-buffer controls from the public pane.
   Merely changing `roi_out[]` to `roi_out[3]` in C does not fix the generated VI.
6. **Call the DLL** with all eight arguments; wire the return value to status.
   Pass the resulting three- and four-element array wires directly to the
   respective output indicators. Their Array Size must be 3 and 4 on every
   path, including locally rejected input and DLL failures.
7. **Trim the crop conditionally.** Only for status=0, or status=-4 with
   `roi_out[2]>0`, compute `n=I64(size)*I64(size)` and verify n<=capacity64.
   Use Array Subset, index **0**, length checked I32(n), on parameter 5's output.
   Return that exact 1D U16 subset. For other statuses return an empty U16 array.
   In particular -2 reports geometry but has **no valid crop**. Do not trim or
   display the uninitialized/previous buffer on -2.
8. If a 2D display is useful, reshape a successful crop to size×size internally
   or outside the VI; the requested connector-pane crop stays **1D U16**.

LabVIEW owns all arrays. The DLL only reads/writes their supplied storage; it
never resizes LabVIEW Array Data Pointers using `malloc`, `realloc`, `new` or the
LabVIEW Memory Manager. Actual array lengths must match the declared capacity.

For a later optimization, retain the checked worst-case crop buffer in a shift
register inside an acquisition/analysis loop and reinitialize only when frame
dimensions change. Keep capacity private, allocate separate I32[3]/DBL[4]
results, trim the valid region each iteration and honor dataflow/in-place
semantics so stale output never escapes after an error. This does not require a
new DLL ABI or extra public connector inputs.

## ROI, outputs and errors

`[0,0,0]` alone means automatic detection. A manual `[left,top,size]` requires
left/top>=0, size>0, size<=W,H, left<=W-size and top<=H-size. Valid manual ROIs
are copied exactly, including those touching an image edge. No clamping,
expansion or full-frame statistics occur. A valid tiny ROI can be cropped yet
lack enough rings for analysis; that is -4 rather than invalid geometry.

Both modes analyze their square crop using identical crop-local normalization,
robust gradient center fitting, squared-radius profiles, peak spacing/phase and
linewidth/finesse estimation. Source and returned U16 values remain unchanged.
`roi_out=[left,top,size]`; `metrics_out=[ring_phase,center_x,center_y,finesse_proxy]`.
Centers are **relative to the crop**; add left/top for source-frame coordinates.
Bright peaks satisfy `r²/spacing + ring_phase = integer`, phase in [0,1).
Finesse is a dimensionless spacing/FWHM image proxy, not calibrated intrinsic
optical finesse or absolute cavity displacement.

| status | Handling |
| --- | --- |
| 0 | all four metrics and exactly size² cropped pixels are valid |
| -1 | invalid input/ROI; zero ROI, NaN metrics, crop untouched |
| -2 | insufficient capacity; valid retry geometry, NaN metrics, crop untouched |
| -3 | auto could not find a region; zero ROI, NaN metrics, crop untouched |
| -4 | insufficient reliable rings/resource failure; NaN metrics; positive size means diagnostic crop is valid, otherwise no crop |
| -5 | invalid method on the selector only; zero ROI, NaN metrics, no crop |

Detected overlapping storage returns -1 before any output initialization;
outputs are invalid in this case. The wrapper must always use separate arrays.
Never use NaNs or measurements from a failed call for instrument control.

## Manual LabVIEW verification checklist

1. Call `FP_GetApiVersion` with no arguments and I32 return: expect **2**.
   Check `dumpbin /exports` shows exactly the three functions in the header and
   `/headers` shows 14C for x86 or 8664 for x64. A bitness mismatch can appear
   as load error 193; a missing MSVC runtime as error 126.
2. Reimport/edit the U16 node with the new fourth argument. Verify U16 pointers,
   cdecl, parameter order and constant minimum lengths 3/4. Confirm the public
   VI has four inputs and four outputs; capacity has no connector terminal.
3. Use 32×32 zeros, empty roi_in: wrapper supplies [0,0,0], status=-3,
   roi_out length 3 with zeros, metrics_out length 4 with NaNs, crop empty.
4. Same image, manual [0,0,32]: status=-4, ROI unchanged, 1024 zero U16 crop
   elements, four NaNs. This distinguishes detection from manual analysis.
5. Use a known synthetic ring image. Test automatic mode and then feed its
   roi_out back as manual roi_in. Compare all metrics and every cropped pixel;
   equivalent crops should match. Confirm center coordinates are crop-relative.
6. Test a boundary ROI, invalid negative/out-of-frame ROI and roi_in lengths
   1/2/4. Invalid calls must return errors without exposing a stale crop.
7. Verify crop samples above 255 and at 0/65535. Observe Array Size results:
   3 for ROI, 4 for metrics, size² for a valid crop, never 8194 for metadata.
8. Validate frame-loop buffer reuse and actual camera performance separately.

`examples/python_ctypes_test.py` independently checks the compiled cdecl
signature and reference allocation/trim flow, including empty ROI conversion.
It is an executable reference for the wiring, not proof of LabVIEW execution.
Run it with Python matching the DLL's architecture; Python is not required on
the instrument PC. Real camera accuracy and LabVIEW verification remain manual.

## Threading and meaningful timing on the instrument

Native concurrent tests run A/B/C/D calls simultaneously with separate output
and ROI buffers and shared read-only input; results match sequential calls.
All scratch memory and diagnostics are per call, without mutable global/history
state. Set the nodes to **Run in any thread**. The caller must keep arrays alive
until return, avoid overlapping input/output storage, and use separate buffers
for concurrent nodes. Actual LabVIEW threading remains an instrument check.

To measure on the user's machine, preallocate arrays and normalize ROI before
the measured loop. Time only the node with a high-resolution timer and dataflow
or sequence dependencies forcing the start/stop around it. Warm up, collect
many calls and compute median and p95. Node timing can still include LabVIEW
marshalling/copies. Separately time buffer allocation, acquisition, array subset/
reshape and display updates, placing front-panel refresh outside the timed node
loop. Reuse buffers in shift registers and check LabVIEW's buffer-allocation
visualization for unexpected copies; retain the exact crop/metadata contracts.

Run the native Release fp_methods_benchmark on that same computer with identical
frame/ROI to measure DLL processing separately. Compare original and selector
method 1; compare manual with auto to measure detection overhead. Do not infer
native speed from the user's earlier approximately 50 ms manual / 60 ms auto
LabVIEW observations, or compare this cloud's numbers to instrument timings as
an optimization ratio. See methods_and_performance.md for raw reports and limits.
