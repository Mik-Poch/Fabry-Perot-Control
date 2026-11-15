#!/usr/bin/env python3
"""cdecl bindings and optional experimental BMP validation; Python stdlib only.

Usage: python examples/fringe_asymmetry.py build/libFPAnalyzer.so data/test_frames
8-bit grayscale indexed BMP and 24/32-bit grayscale RGB BMP are supported.
Samples are copied to U16 unchanged (0..255 stays 0..255). Color, compressed,
16-bit RGB and non-grayscale palette inputs are rejected, never silently quantized.
For native U16 cameras, call bind()/analyze() with original 16-bit samples.
"""
import ctypes as ct
import json
import math
from pathlib import Path
import struct
import sys

NAMES = ('Image0803.bmp', 'Image0792.bmp', 'Image0699.bmp', 'Image0855.bmp')
FIELDS = ('phase', 'center_x', 'center_y', 'finesse_proxy', 'Ax', 'Ay', 'Wx', 'Wy',
          'brightness_magnitude', 'brightness_direction_deg', 'width_magnitude',
          'width_direction_deg', 'correlation', 'valid_rings', 'valid_sectors', 'harmonic_RMS')


def bind(path):
    dll = ct.CDLL(str(Path(path).resolve()))
    call = dll.FP_AnalyzeFringeAsymmetryU16  # feature detection; API version stays 2
    call.argtypes = [ct.POINTER(ct.c_ushort), ct.c_int, ct.c_int, ct.POINTER(ct.c_int),
                     ct.POINTER(ct.c_int), ct.POINTER(ct.c_double), ct.POINTER(ct.c_double)]
    call.restype = ct.c_int
    return call


def analyze(call, samples, width, height, roi=(0, 0, 0)):
    if width <= 0 or height <= 0 or width*height > 2**31-1 or len(samples) != width*height:
        raise ValueError('image dimensions/length')
    if len(roi) != 3 or any(not 0 <= x <= 65535 for x in samples):
        raise ValueError('ROI length/U16 range')
    image = (ct.c_ushort*len(samples))(*samples)
    selected, actual = (ct.c_int*3)(*roi), (ct.c_int*3)()
    summary, sectors = (ct.c_double*16)(), (ct.c_double*64)()
    status = call(image, width, height, selected, actual, summary, sectors)
    return {'status': status, 'roi': list(actual), 'metrics': dict(zip(FIELDS, summary)),
            'sectors': [list(sectors[4*s:4*s+4]) for s in range(16)]}


def load_bmp(path):
    data = Path(path).read_bytes()
    if len(data) < 54 or data[:2] != b'BM':
        raise ValueError('not a BMP')
    offset, = struct.unpack_from('<I', data, 10)
    dib, width, signed_height, planes, bits, compression = struct.unpack_from('<IiiHHI', data, 14)
    if dib < 40 or width <= 0 or not signed_height or planes != 1 or compression != 0 or bits not in (8, 24, 32):
        raise ValueError('unsupported BMP layout (need uncompressed grayscale 8/24/32-bit)')
    height = abs(signed_height)
    stride = ((width*bits+31)//32)*4
    if offset < 14+dib or offset+stride*height > len(data):
        raise ValueError('truncated BMP')
    palette = None
    if bits == 8:
        used, = struct.unpack_from('<I', data, 46)
        used = used or 256
        if used > 256 or 14+dib+used*4 > offset:
            raise ValueError('invalid palette')
        palette = []
        for i in range(used):
            b, g, r, _ = struct.unpack_from('<BBBB', data, 14+dib+4*i)
            if r != g or g != b:
                raise ValueError('color palette is not grayscale')
            palette.append(r)
    samples = []
    for y in range(height):
        row = offset+(height-1-y if signed_height > 0 else y)*stride
        for x in range(width):
            if bits == 8:
                index = data[row+x]
                if index >= len(palette):
                    raise ValueError('invalid palette index')
                samples.append(palette[index])
            else:
                b, g, r = data[row+x*(bits//8):row+x*(bits//8)+3]
                if r != g or g != b:
                    raise ValueError('RGB input is not grayscale')
                samples.append(r)
    return samples, width, height


def finite_json(value):
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if isinstance(value, dict):
        return {k: finite_json(v) for k, v in value.items()}
    if isinstance(value, list):
        return [finite_json(v) for v in value]
    return value


def main():
    if len(sys.argv) != 3:
        raise SystemExit('Usage: python fringe_asymmetry.py FPAnalyzer.dll directory_with_BMPs')
    call = bind(sys.argv[1])
    missing, failed = [], False
    for name in NAMES:
        path = Path(sys.argv[2])/name
        if not path.exists():
            missing.append(name)
            continue
        try:
            pixels, w, h = load_bmp(path)
            result = analyze(call, pixels, w, h)
            print(json.dumps(finite_json({'image': name, **result}), allow_nan=False))
            failed |= result['status'] != 0
        except (ValueError, OSError, struct.error) as error:
            print(f'FAIL {name}: {error}', file=sys.stderr)
            failed = True
    if missing:
        print('SKIP unavailable experimental inputs: '+', '.join(missing), file=sys.stderr)
    return 1 if failed else (77 if missing else 0)


if __name__ == '__main__':
    sys.exit(main())
