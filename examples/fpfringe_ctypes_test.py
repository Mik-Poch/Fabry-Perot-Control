#!/usr/bin/env python3
"""Matching-bitness C/cdecl and frozen/current legacy comparison, stdlib only.
Usage: python fpfringe_ctypes_test.py FPFringe.dll FPAnalyzer.dll
The legacy argument may also be a retained pre-refactor library of the same ABI.
"""
import ctypes as c
import math
from pathlib import Path
import sys


def bind(path):
    lib = c.CDLL(str(Path(path).resolve()))
    u, i, d = c.POINTER(c.c_ushort), c.POINTER(c.c_int), c.POINTER(c.c_double)
    definitions = {
        'FPF_GetApiVersion': [],
        'FPF_FindCenteredRoiU16': [u, c.c_int, c.c_int, u, c.c_int, i],
        'FPF_AnalyzeU16': [u, c.c_int, c.c_int, c.c_int, d],
        'FPF_AnalyzeAsymmetryU16': [u, c.c_int, c.c_int, d, d],
    }
    for name, types in definitions.items():
        call = getattr(lib, name)
        call.argtypes, call.restype = types, c.c_int
    return lib


def render(bits=16, width=320, height=320):
    image = (c.c_ushort*(width*height))()
    maximum = (1 << bits)-1
    for y in range(height):
        for x in range(width):
            dx, dy = x-(width-1)/2-.3, y-(height-1)/2+.2
            theta = math.atan2(dy, dx)
            w = .24*(1-.18*math.cos(theta)+.12*math.sin(theta))
            f = 1/math.sin(math.pi*w/2)**2-2
            b = 1/(1+f)
            a = math.sin(math.pi*((dx*dx+dy*dy)/3600+.27))
            value = maximum*(.05+.6*(1+.25*math.cos(theta)-.15*math.sin(theta))*(1/(1+f*a*a)-b)/(1-b))
            image[y*width+x] = round(value)
    return image


def same(a, b):
    return len(a) == len(b) and all(x == y or (math.isnan(x) and math.isnan(y)) for x, y in zip(a, b))


def main():
    lib = bind(sys.argv[1])
    old = c.CDLL(str(Path(sys.argv[2]).resolve()))
    u, i, d = c.POINTER(c.c_ushort), c.POINTER(c.c_int), c.POINTER(c.c_double)
    old.FP_AnalyzeFrameU16WithMethod.argtypes = [u,c.c_int,c.c_int,i,c.c_int,u,c.c_int,i,d]
    old.FP_AnalyzeFringeAsymmetryU16.argtypes = [u,c.c_int,c.c_int,i,i,d,d]
    assert lib.FPF_GetApiVersion() == 4
    try:
        lib.FPF_CheckFringesU16
    except AttributeError:
        pass
    else:
        raise AssertionError('removed checker is still exported')
    compared = 0
    for bits in (8, 10, 12, 14, 16):
        image = render(bits)
        roi, actual = (c.c_int*3)(0,0,320), (c.c_int*3)()
        crop = (c.c_ushort*(320*320))()
        for method in range(4):
            previous, current = (c.c_double*4)(), (c.c_double*4)()
            before = old.FP_AnalyzeFrameU16WithMethod(image,320,320,roi,method,crop,320*320,actual,previous)
            after = lib.FPF_AnalyzeU16(image,320,320,method,current)
            assert before == after, (bits, method, before, after)
            assert same(previous, current), (bits, method, list(previous), list(current))
            compared += 4
        previous, current = (c.c_double*16)(), (c.c_double*16)()
        ps, cs = (c.c_double*64)(), (c.c_double*64)()
        before = old.FP_AnalyzeFringeAsymmetryU16(image,320,320,roi,actual,previous,ps)
        after = lib.FPF_AnalyzeAsymmetryU16(image,320,320,current,cs)
        assert before == after == 0
        assert same(previous, current) and same(ps, cs)
        compared += 80
        output, roi_pixels = (c.c_int*3)(), (c.c_ushort*(320*320+2))()
        roi_pixels[0], roi_pixels[-1] = 123, 456
        pointer = c.cast(c.byref(roi_pixels,c.sizeof(c.c_ushort)),c.POINTER(c.c_ushort))
        automatic, legacy_roi = (c.c_int*3)(), (c.c_int*3)()
        legacy_pixels = (c.c_ushort*(320*320))()
        legacy_metrics = (c.c_double*4)()
        expected_status = old.FP_AnalyzeFrameU16WithMethod(image,320,320,automatic,0,legacy_pixels,320*320,legacy_roi,legacy_metrics)
        status = lib.FPF_FindCenteredRoiU16(image,320,320,pointer,320*320,output)
        assert status == expected_status and list(output) == list(legacy_roi)
        assert list(roi_pixels[1:-1]) == list(legacy_pixels)
        assert roi_pixels[0] == 123 and roi_pixels[-1] == 456
    for w,h in ((400,320),(320,400)):
        image = render(width=w,height=h)
        m = (c.c_double*4)()
        assert lib.FPF_AnalyzeU16(image,w,h,1,m) == 0
        assert abs(m[1]-((w-1)/2+.3)) < .7 and abs(m[2]-((h-1)/2-.2)) < .7
    print(f'PASS FPFringe ctypes: four exports, API 4, rectangular coordinates, {compared} exact legacy output comparisons (NaNs included)')


if __name__ == '__main__':
    main()
