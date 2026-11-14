#!/usr/bin/env python3
"""Optional five-frame diagnostic, stdlib only; no private image data committed.
Usage: python fpfringe_validate.py FPFringe.dll FPAnalyzer.dll data/test_frames
External comparison crops are explicit in this tool, never hidden in FPFringe.
"""
import ctypes as c
import json
from pathlib import Path
import sys
from fpfringe_ctypes_test import bind, same
from fringe_asymmetry import load_bmp, finite_json

NAMES = ('Image0803.bmp','Image0792.bmp','Image0699.bmp','Image0855.bmp','Image0020.bmp')


def main():
    new = bind(sys.argv[1])
    old = c.CDLL(str(Path(sys.argv[2]).resolve()))
    u,i,d = c.POINTER(c.c_ushort),c.POINTER(c.c_int),c.POINTER(c.c_double)
    old.FP_AnalyzeFrameU16WithMethod.argtypes = [u,c.c_int,c.c_int,i,c.c_int,u,c.c_int,i,d]
    old.FP_AnalyzeFringeAsymmetryU16.argtypes = [u,c.c_int,c.c_int,i,i,d,d]
    missing, errors = [], 0
    for name in NAMES:
        path = Path(sys.argv[3])/name
        if not path.exists():
            missing.append(name)
            continue
        pixels,w,h = load_bmp(path)
        image = (c.c_ushort*len(pixels))(*pixels)
        roi,center,boolean = (c.c_int*4)(),(c.c_double*2)(),c.c_int()
        rs = new.FPF_FindCenteredRoiU16(image,w,h,0,roi,center)
        bs = new.FPF_CheckFringesU16(image,w,h,c.byref(boolean))
        results = {'image':name,'dimensions':[w,h],'roi_status':rs,'roi':list(roi),'center_full':list(center),
                   'screen_status':bs,'analyzable':boolean.value,'full_frame_methods':[]}
        for method in range(4):
            m=(c.c_double*4)()
            status=new.FPF_AnalyzeU16(image,w,h,method,m)
            results['full_frame_methods'].append({'method':method,'status':status,'metrics':list(m)})
        a,s=(c.c_double*16)(),(c.c_double*64)()
        results['asymmetry_status']=new.FPF_AnalyzeAsymmetryU16(image,w,h,a,s)
        results['summary'],results['sectors']=list(a),list(s)
        if rs==0:
            left,top,size,_=roi
            crop_values=[pixels[(y+top)*w+x+left] for y in range(size) for x in range(size)]
            crop=(c.c_ushort*len(crop_values))(*crop_values)
            outcrop=(c.c_ushort*len(crop_values))()
            request,actual=(c.c_int*3)(0,0,size),(c.c_int*3)()
            comparisons=[]
            for method in range(4):
                before,after=(c.c_double*4)(),(c.c_double*4)()
                os=old.FP_AnalyzeFrameU16WithMethod(crop,size,size,request,method,outcrop,size*size,actual,before)
                ns=new.FPF_AnalyzeU16(crop,size,size,method,after)
                matches=os==ns and same(before,after)
                errors+=not matches
                comparisons.append({'method':method,'legacy_status':os,'new_status':ns,'exact_match':matches})
            olda,olds=(c.c_double*16)(),(c.c_double*64)()
            newa,news=(c.c_double*16)(),(c.c_double*64)()
            os=old.FP_AnalyzeFringeAsymmetryU16(crop,size,size,request,actual,olda,olds)
            ns=new.FPF_AnalyzeAsymmetryU16(crop,size,size,newa,news)
            matches=os==ns and same(olda,newa) and same(olds,news)
            errors+=not matches
            comparisons.append({'asymmetry':True,'legacy_status':os,'new_status':ns,'exact_match':matches})
            results['explicit_square_comparisons']=comparisons
        print(json.dumps(finite_json(results),allow_nan=False))
    if missing:
        print('SKIP unavailable experimental images: '+', '.join(missing),file=sys.stderr)
    return 1 if errors else (77 if missing else 0)


if __name__ == '__main__':
    sys.exit(main())
