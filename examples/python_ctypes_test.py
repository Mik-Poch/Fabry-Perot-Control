#!/usr/bin/env python3
"""Independent shared-library ABI/functional test; Python stdlib only.

Windows: python examples/python_ctypes_test.py build/Release/FPAnalyzer.dll
Linux:   python3 examples/python_ctypes_test.py build/libFPAnalyzer.so
Python is a development tool only; the native DLL does not use Python.
"""
import ctypes as ct
import math
from pathlib import Path
import sys
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def test_u16(dll):
    require(ct.sizeof(ct.c_ushort) == 2, "U16 ABI")
    call = dll.FP_AnalyzeFrameU16
    call.argtypes = [ct.POINTER(ct.c_ushort), ct.c_int, ct.c_int,
                     ct.POINTER(ct.c_ushort), ct.c_int,
                     ct.POINTER(ct.c_int), ct.POINTER(ct.c_double)]
    call.restype = ct.c_int
    width, height = 384, 320
    cx, cy, radius, spacing, phase = 185.35, 162.7, 125, 2200, 0.73
    for amplitude, background in ((800, 20), (3200, 80), (52000, 1000), (64, 40000)):
        pixels = (ct.c_ushort * (width * height))()
        for y in range(height):
            for x in range(width):
                q = (x-cx)**2 + (y-cy)**2
                envelope = min(1.0, max(0.0, (radius-math.sqrt(q))/12.0))
                pixels[y*width+x] = round(background + envelope*amplitude /
                    (1+20*math.sin(math.pi*(q/spacing+phase))**2))
        crop = (ct.c_ushort * (width*height+2))(*([0xA55A]*(width*height+2)))
        # Leading/trailing canaries exercise element counts versus byte counts.
        output = ct.cast(ct.byref(crop, ct.sizeof(ct.c_ushort)), ct.POINTER(ct.c_ushort))
        roi, metrics = (ct.c_int*3)(), (ct.c_double*4)()
        require(call(pixels,width,height,output,0,roi,metrics) == -2, "U16 zero capacity")
        required = roi[2]**2
        require(required > 0 and all(math.isnan(v) for v in metrics), "U16 capacity outputs")
        require(call(pixels,width,height,output,required-1,roi,metrics) == -2, "U16 one element short")
        require(all(v == 0xA55A for v in crop), "U16 no partial writes")
        require(call(pixels,width,height,output,required,roi,metrics) == 0, "U16 exact capacity in elements")
        left, top, size = roi
        require(0 < size <= min(width,height) and 0 <= left <= width-size and
                0 <= top <= height-size, "U16 ROI")
        require(abs(left+metrics[1]-cx) < 0.7 and abs(top+metrics[2]-cy) < 0.7, "U16 center")
        error = abs(metrics[0]-phase)
        require(0 <= metrics[0] < 1 and min(error,1-error) < 0.025, "U16 phase")
        require(math.isfinite(metrics[3]) and metrics[3] > 1, "U16 finesse")
        for y in range(size):
            require(list(crop[1+y*size:1+(y+1)*size]) ==
                    list(pixels[(y+top)*width+left:(y+top)*width+left+size]), "exact native U16 crop")
        require(crop[0] == 0xA55A and all(v == 0xA55A for v in crop[1+size*size:]), "U16 crop guards")
        require(call(pixels,65536,65536,output,required,roi,metrics) == -1, "U16 dimensions overflow")
        require(list(roi) == [0,0,0] and all(math.isnan(v) for v in metrics), "U16 invalid outputs")
    print("PASS U16 ctypes: 10/12/16-bit and low-bit contrast, exact crop, metrics, capacity and invalid ABI")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python python_ctypes_test.py /path/to/FPAnalyzer.dll")
    # CDLL uses cdecl (WinDLL would select stdcall).
    dll = ct.CDLL(str(Path(sys.argv[1]).resolve()))
    dll.FP_GetApiVersion.argtypes = []
    dll.FP_GetApiVersion.restype = ct.c_int
    dll.FP_AnalyzeFrameU8.argtypes = [
        ct.POINTER(ct.c_ubyte), ct.c_int, ct.c_int,
        ct.POINTER(ct.c_ubyte), ct.c_int,
        ct.POINTER(ct.c_int), ct.POINTER(ct.c_double),
    ]
    dll.FP_AnalyzeFrameU8.restype = ct.c_int
    require(ct.sizeof(ct.c_int) == 4, "I32 ABI")
    require(dll.FP_GetApiVersion() == 1, "API version")

    width, height = 384, 320
    cx, cy, radius, spacing, phase = 185.35, 162.7, 125, 2200, 0.73
    pixels = (ct.c_ubyte * (width * height))()
    for y in range(height):
        for x in range(width):
            q = (x - cx)**2 + (y - cy)**2
            envelope = min(1.0, max(0.0, (radius - math.sqrt(q)) / 12.0))
            value = 5 + envelope * 190 / (1 + 20 * math.sin(math.pi * (q / spacing + phase))**2)
            pixels[y * width + x] = round(value)
    crop = (ct.c_ubyte * (width * height + 1))(*([0xA5] * (width * height + 1)))
    roi, metrics = (ct.c_int * 3)(), (ct.c_double * 4)()
    call = dll.FP_AnalyzeFrameU8
    require(call(pixels, width, height, crop, 0, roi, metrics) == -2, "capacity status")
    require(all(v == 0xA5 for v in crop), "no capacity => no writes")
    require(all(math.isnan(v) for v in metrics), "failure NaNs")
    start = time.perf_counter()
    status = call(pixels, width, height, crop, width * height, roi, metrics)
    elapsed_ms = (time.perf_counter() - start) * 1000
    require(status == 0, f"analysis status={status}, ROI={list(roi)}")
    left, top, size = roi
    require(0 <= left <= width - size and 0 <= top <= height - size, "ROI bounds")
    require(0 < size <= min(width, height), "square size")
    require(abs(left + metrics[1] - cx) < 0.7, "center X")
    require(abs(top + metrics[2] - cy) < 0.7, "center Y")
    require(0 <= metrics[0] < 1, "phase range")
    error = abs(metrics[0] - phase)
    require(min(error, 1 - error) < 0.025, "phase truth")
    require(math.isfinite(metrics[3]) and metrics[3] > 1, "finesse")
    for y in range(size):
        require(bytes(crop[y*size:(y+1)*size]) == bytes(pixels[(y+top)*width+left:(y+top)*width+left+size]), "crop copy")
    require(all(v == 0xA5 for v in crop[size*size:]), "crop tail canary")
    require(call(pixels, -1, height, crop, width*height, roi, metrics) == -1, "invalid dimension")
    require(list(roi) == [0, 0, 0] and all(math.isnan(v) for v in metrics), "invalid outputs")
    print(f"PASS ctypes: version=1, ROI/crop/phase/center/finesse/capacity/invalid ABI; analysis_ms={elapsed_ms:.3f}")
    test_u16(dll)


if __name__ == "__main__":
    main()
