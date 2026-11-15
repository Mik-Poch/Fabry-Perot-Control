#!/usr/bin/env python3
"""Matching-bitness shared-library tests, independent synthetic renderer."""
import ctypes as ct
import math
from pathlib import Path
import struct
import sys
import tempfile
from fringe_asymmetry import bind, analyze, load_bmp


def main():
    call = bind(sys.argv[1])
    n, cx, cy = 320, 153.3, 164.7
    pixels = []
    for y in range(n):
        for x in range(n):
            dx, dy = x-cx, y-cy
            theta = math.atan2(dy, dx)
            signal = (1+.25*math.cos(theta)-.15*math.sin(theta))
            value = 1000+30000*signal/(1+5*math.sin(math.pi*((dx*dx+dy*dy)/3600+.27))**2)
            pixels.append(round(value))
    result = analyze(call, pixels, n, n, (0, 0, n))
    assert result['status'] == 0, result
    m = result['metrics']
    assert abs(m['Ax']-.25) < .04 and abs(m['Ay']+.15) < .04, m
    assert abs(m['Wx']) < .025 and abs(m['Wy']) < .025, m
    assert m['valid_rings'] >= 3 and m['valid_sectors'] >= 10
    class Guard(ct.Structure):
        _fields_ = [('before', ct.c_double), ('m', ct.c_double*16),
                    ('middle', ct.c_double), ('s', ct.c_double*64), ('after', ct.c_double)]
    g = Guard();g.before, g.middle, g.after = 123, 456, 789
    image, roi, actual = (ct.c_ushort*len(pixels))(*pixels), (ct.c_int*3)(0, 0, n), (ct.c_int*3)()
    assert call(image, n, n, roi, actual, g.m, g.s) == 0
    assert (g.before, g.middle, g.after) == (123, 456, 789)
    assert list(g.m) == list(m.values())
    auto = analyze(call, pixels, n, n)
    assert auto['status'] == 0, auto
    assert analyze(call, pixels, n, n, auto['roi']) == auto
    assert call(None, n, n, roi, actual, g.m, g.s) == -1
    assert all(math.isnan(v) for v in g.m)
    # BMP byte order, row padding, palette, top-down orientation, U16 preservation.
    with tempfile.TemporaryDirectory() as tmp:
        for bits in (8, 24, 32):
            for topdown in (False, True):
                values = [[0, 17, 255], [250, 123, 3]]
                rows = values if topdown else list(reversed(values))
                stride = ((3*bits+31)//32)*4
                body = bytearray()
                for row in rows:
                    raw = bytes(row) if bits == 8 else b''.join(bytes([v]*3+([0] if bits == 32 else [])) for v in row)
                    body += raw+bytes(stride-len(raw))
                palette = b''.join(bytes([v, v, v, 0]) for v in range(256)) if bits == 8 else b''
                offset = 54+len(palette)
                header = b'BM'+struct.pack('<IHHI', offset+len(body), 0, 0, offset)
                dib = struct.pack('<IiiHHIIiiII', 40, 3, -2 if topdown else 2, 1, bits, 0, len(body), 0, 0, 0, 0)
                path = Path(tmp)/'fixture.bmp';path.write_bytes(header+dib+palette+body)
                samples, w, h = load_bmp(path)
                assert (w, h, samples) == (3, 2, values[0]+values[1])
    print('PASS asymmetry ctypes: signed brightness, no false width, auto/manual, canaries, failures, BMP preservation')


if __name__ == '__main__':
    main()
