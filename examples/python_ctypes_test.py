#!/usr/bin/env python3
"""API v2 cdecl ABI/functional tests, Python stdlib only.

Use Python matching the DLL's architecture. This is not a generated LabVIEW VI.
"""
import ctypes as ct
import math
from pathlib import Path
import sys


AUTO = (ct.c_int * 3)(0, 0, 0)
SENTINEL = 0xA55A


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class RoiGuard(ct.Structure):
    _fields_ = [("before", ct.c_int), ("values", ct.c_int * 3), ("after", ct.c_int)]


class MetricsGuard(ct.Structure):
    _fields_ = [("before", ct.c_double), ("values", ct.c_double * 4), ("after", ct.c_double)]


def frame(width, height, amplitude, background):
    pixels = (ct.c_ushort * (width * height))()
    for y in range(height):
        for x in range(width):
            q = (x-185.35)**2 + (y-162.7)**2
            envelope = min(1.0, max(0.0, (125-math.sqrt(q))/12.0))
            pixels[y*width+x] = round(background + envelope*amplitude /
                (1+20*math.sin(math.pi*(q/2200+0.73))**2))
    return pixels


def wrapper(call, pixels, width, height, roi_in):
    """Reference allocation/trim flow for the four-input LabVIEW wrapper.

    The bound C function is internal. ROI length is checked here, never in C.
    Native status -4 can retain a diagnostic crop; other errors return no crop.
    """
    roi, metrics = (ct.c_int*3)(), (ct.c_double*4)(*[math.nan]*4)
    if (width <= 0 or height <= 0 or width*height > 2**31-1 or
            len(pixels) != width*height or len(roi_in) not in (0, 3)):
        return [], list(roi), list(metrics), -1
    selected = (ct.c_int*3)(*(roi_in or [0, 0, 0]))
    capacity = min(width, height)**2
    crop = (ct.c_ushort*capacity)()
    status = call(pixels, width, height, selected, crop, capacity, roi, metrics)
    count = roi[2]**2 if status in (0, -4) and roi[2] > 0 else 0
    return list(crop[:count]), list(roi), list(metrics), status


def test_u16(call):
    width, height = 384, 320
    for amplitude, background in ((800, 20), (3200, 80), (52000, 1000), (64, 40000)):
        pixels = frame(width, height, amplitude, background)
        crop = (ct.c_ushort * (width*height+2))(*([SENTINEL]*(width*height+2)))
        output = ct.cast(ct.byref(crop, ct.sizeof(ct.c_ushort)), ct.POINTER(ct.c_ushort))
        roi = RoiGuard(123, (ct.c_int*3)(), 456)
        metrics = MetricsGuard(123, (ct.c_double*4)(), 456)

        def invoke(selected, capacity):
            status = call(pixels, width, height, selected, output, capacity, roi.values, metrics.values)
            require((roi.before, roi.after) == (123, 456), "exactly three I32 outputs")
            require((metrics.before, metrics.after) == (123, 456), "exactly four DBL outputs")
            return status

        require(invoke(AUTO, 0) == -2, "auto zero capacity")
        required = roi.values[2]**2
        require(required > 0 and all(math.isnan(v) for v in metrics.values), "capacity outputs")
        require(invoke(AUTO, required-1) == -2, "auto short capacity")
        require(all(v == SENTINEL for v in crop), "no partial writes")
        require(invoke(AUTO, required) == 0, "auto exact element capacity")
        selected = (ct.c_int*3)(*roi.values)
        reference_metrics = list(metrics.values)
        require(invoke(selected, required) == 0, "manual equivalent ROI")
        require(list(roi.values) == list(selected), "manual ROI unchanged")
        require(list(metrics.values) == reference_metrics, "both modes have identical metrics")
        left, top, size = roi.values
        require(0 < size <= min(width, height) and 0 <= left <= width-size and
                0 <= top <= height-size, "ROI bounds")
        require(abs(metrics.values[1]-(185.35-left)) < 0.7 and
                abs(metrics.values[2]-(162.7-top)) < 0.7, "crop-relative center truth")
        error = abs(metrics.values[0]-0.73)
        require(0 <= metrics.values[0] < 1 and min(error, 1-error) < 0.025, "phase truth")
        require(math.isfinite(metrics.values[3]) and metrics.values[3] > 1, "finesse")
        for y in range(size):
            require(list(crop[1+y*size:1+(y+1)*size]) ==
                    list(pixels[(y+top)*width+left:(y+top)*width+left+size]), "exact native U16 crop")
        require(crop[0] == SENTINEL and all(v == SENTINEL for v in crop[1+size*size:]), "crop guards")
        require(invoke(selected, required-1) == -2, "manual short capacity")
        require(list(roi.values) == list(selected), "manual retry geometry")

        for invalid in ((1, 0, 0), (-1, 0, 10), (0, -1, 10), (0, 0, -1),
                        (width-9, 0, 10), (0, height-9, 10), (0, 0, 2**31-1)):
            require(invoke((ct.c_int*3)(*invalid), required) == -1, "invalid ROI rejected")
            require(list(roi.values) == [0, 0, 0] and all(math.isnan(v) for v in metrics.values),
                    "invalid ROI outputs")
        require(invoke(None, required) == -1, "null ROI input")
        require(call(pixels, 65536, 65536, AUTO, output, required, roi.values, metrics.values) == -1,
                "dimensions overflow")
        wrapped = wrapper(call, pixels, width, height, [])
        require(wrapped[3] == 0 and wrapped[1] == list(selected), "empty wrapper ROI becomes auto")
        require(len(wrapped[0]) == required and len(wrapped[1]) == 3 and len(wrapped[2]) == 4,
                "wrapper trims to exact lengths")
        require(wrapper(call, pixels, width, height, list(selected)) == wrapped, "four-input wrapper manual")
        require(wrapper(call, pixels, width, height, [0])[3] == -1, "wrapper rejects wrong ROI length")
    print("PASS ctypes: auto/manual, precision, truth, exact lengths, canaries, capacity and wrapper flow")


def boundaries(call):
    width, height = 40, 32
    pixels = (ct.c_ushort*(width*height))(*(i*51 for i in range(width*height)))
    pixels[0], pixels[-1] = 0, 65535
    roi, metrics = (ct.c_int*3)(), (ct.c_double*4)()
    for request in ((0, 0, 32), (8, 0, 32), (39, 31, 1)):
        selected = (ct.c_int*3)(*request)
        count = request[2]**2
        crop = (ct.c_ushort*(count+2))(*([SENTINEL]*(count+2)))
        out = ct.cast(ct.byref(crop, 2), ct.POINTER(ct.c_ushort))
        status = call(pixels, width, height, selected, out, count, roi, metrics)
        require(status in (0, -4) and list(roi) == list(request), "boundary geometry accepted")
        expected = [pixels[(y+request[1])*width+x+request[0]]
                    for y in range(request[2]) for x in range(request[2])]
        require(list(crop[1:-1]) == expected and crop[0] == crop[-1] == SENTINEL,
                "boundary U16 exact crop and guards")
    blank = (ct.c_ushort*1024)()
    crop = (ct.c_ushort*1024)(*([SENTINEL]*1024))
    require(call(blank, 32, 32, AUTO, crop, 1024, roi, metrics) == -3, "blank automatic no region")
    require(list(roi) == [0, 0, 0] and all(math.isnan(v) for v in metrics), "blank auto outputs")
    require(call(blank, 32, 32, (ct.c_int*3)(0, 0, 32), crop, 1024, roi, metrics) == -4,
            "blank manual bypasses detection and retains diagnostic crop")
    require(list(roi) == [0, 0, 32] and all(v == 0 for v in crop), "manual diagnostic crop")
    print("PASS ctypes: boundary ROI, U16 endpoints and failure outputs")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python python_ctypes_test.py /path/to/FPAnalyzer.dll")
    dll = ct.CDLL(str(Path(sys.argv[1]).resolve())) # cdecl, including Win32
    dll.FP_GetApiVersion.argtypes = []
    dll.FP_GetApiVersion.restype = ct.c_int
    require(ct.sizeof(ct.c_int) == 4 and ct.sizeof(ct.c_ushort) == 2, "I32/U16 ABI")
    require(dll.FP_GetApiVersion() == 2, "breaking API version 2")
    try:
        dll.FP_AnalyzeFrameU8
    except AttributeError:
        pass
    else:
        raise AssertionError("removed U8 API is still exported")
    call = dll.FP_AnalyzeFrameU16
    call.argtypes = [ct.POINTER(ct.c_ushort), ct.c_int, ct.c_int, ct.POINTER(ct.c_int),
                     ct.POINTER(ct.c_ushort), ct.c_int, ct.POINTER(ct.c_int), ct.POINTER(ct.c_double)]
    call.restype = ct.c_int
    test_u16(call)
    boundaries(call)


if __name__ == "__main__":
    main()
