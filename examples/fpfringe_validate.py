#!/usr/bin/env python3
"""Optional six-frame diagnostic, stdlib only; no private image data committed.
Usage: python fpfringe_validate.py FPFringe.dll FPAnalyzer.dll data/test_frames
The ROI function returns the raw crop; analysis functions never crop implicitly.
"""
import ctypes as c
import json
from pathlib import Path
import sys
from fpfringe_ctypes_test import bind, same
from fringe_asymmetry import load_bmp, finite_json

NAMES = ('Image0180.bmp','Image0803.bmp','Image0792.bmp','Image0699.bmp','Image0855.bmp','Image0020.bmp')


def main():
    new = bind(sys.argv[1])
    if new.FPF_GetApiVersion() != 4:
        raise SystemExit("FPFringe API 4 DLL required for the six-argument method-0 ROI call")
    old = c.CDLL(str(Path(sys.argv[2]).resolve()))
    u,i,d = c.POINTER(c.c_ushort),c.POINTER(c.c_int),c.POINTER(c.c_double)
    old.FP_AnalyzeFrameU16WithMethod.argtypes = [u,c.c_int,c.c_int,i,c.c_int,u,c.c_int,i,d]
    old.FP_AnalyzeFringeAsymmetryU16.argtypes = [u,c.c_int,c.c_int,i,i,d,d]
    missing, errors, executed = [], 0, 0
    for name in NAMES:
        path = Path(sys.argv[3])/name
        if not path.exists():
            missing.append(name)
            continue
        pixels,w,h = load_bmp(path)
        image = (c.c_ushort*len(pixels))(*pixels)
        executed += 1
        capacity = min(w,h)**2
        roi,legacy_roi,automatic = (c.c_int*3)(),(c.c_int*3)(),(c.c_int*3)()
        old_pixels = (c.c_ushort*(capacity+2))(*([0xA55A]*(capacity+2)))
        roi_pixels = (c.c_ushort*(capacity+2))(*([0xA55A]*(capacity+2)))
        old_ptr = c.cast(c.byref(old_pixels,2),u)
        new_ptr = c.cast(c.byref(roi_pixels,2),u)
        old_metrics = (c.c_double*4)()
        legacy_status = old.FP_AnalyzeFrameU16WithMethod(image,w,h,automatic,0,old_ptr,capacity,legacy_roi,old_metrics)
        rs = new.FPF_FindCenteredRoiU16(image,w,h,new_ptr,capacity,roi)
        exact = rs==legacy_status and list(roi)==list(legacy_roi) and list(old_pixels)==list(roi_pixels)
        canaries = roi_pixels[0]==roi_pixels[-1]==old_pixels[0]==old_pixels[-1]==0xA55A
        source_unchanged = list(image)==pixels
        errors += not (exact and canaries and source_unchanged)
        results = {'image':name,'api_version':new.FPF_GetApiVersion(),'dimensions':[w,h],
                   'roi_status':rs,'legacy_method0_status':legacy_status,'roi':list(roi),
                   'legacy_roi':list(legacy_roi),'roi_crop_status_exact_match':exact,
                   'canaries_intact':canaries,'source_unchanged':source_unchanged,'full_frame_methods':[]}
        for method in range(4):
            m=(c.c_double*4)()
            status=new.FPF_AnalyzeU16(image,w,h,method,m)
            results['full_frame_methods'].append({'method':method,'status':status,'metrics':list(m)})
        a,s=(c.c_double*16)(),(c.c_double*64)()
        results['asymmetry_status']=new.FPF_AnalyzeAsymmetryU16(image,w,h,a,s)
        results['summary'],results['sectors']=list(a),list(s)
        if rs in (0,-4) and roi[2]>0:
            left,top,size=roi
            expected=[pixels[(y+top)*w+x+left] for y in range(size) for x in range(size)]
            matches = list(new_ptr[:size*size]) == expected
            results['crop_matches_source'],results['valid_crop_pixels'] = matches,size*size
            errors += not matches
            crop = new_ptr
            outcrop=(c.c_ushort*(size*size))()
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
    print(f"Experimental comparisons: executed={executed}, missing={len(missing)}, mismatches={errors}",file=sys.stderr)
    return 1 if errors else (77 if executed==0 else 0)


if __name__ == '__main__':
    sys.exit(main())
