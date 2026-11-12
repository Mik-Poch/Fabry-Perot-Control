extern "C" {
#include "FPAnalyzer.h"
}
#include "Synthetic.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
constexpr unsigned short sentinel = 0xA55A;
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void nanMetrics(const double* metrics) {
    for (int i = 0; i < 4; ++i) check(std::isnan(metrics[i]), "NaN failure metrics");
}
struct Result { int roi[3]; double m[4]; };
Result analyze(const std::vector<unsigned short>& image, const synthetic::Parameters& p) {
    std::vector<unsigned short> crop(image.size()+2, sentinel);
    Result r{};
    check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data()+1,
        static_cast<int>(image.size()),r.roi,r.m) == FP_SUCCESS,"U16 analysis succeeds");
    const int left=r.roi[0],top=r.roi[1],size=r.roi[2];
    check(size>0 && size<=std::min(p.width,p.height) && left>=0 && top>=0 &&
          left+size<=p.width && top+size<=p.height,"ROI bounds");
    for (int y=0;y<size;++y) for (int x=0;x<size;++x)
        check(crop[1+static_cast<std::size_t>(y)*size+x] ==
              image[static_cast<std::size_t>(y+top)*p.width+x+left],"exact U16 crop (no truncation)");
    check(crop.front()==sentinel && std::all_of(crop.begin()+1+size*size,crop.end(),
          [](unsigned short v){return v==sentinel;}),"crop guards and tail untouched");
    check(std::abs(left+r.m[1]-p.cx)<0.7 && std::abs(top+r.m[2]-p.cy)<0.7,"U16 center truth");
    check(r.m[0]>=0 && r.m[0]<1 && synthetic::phaseError(r.m[0],p.phase)<0.025,"U16 phase truth");
    check(std::isfinite(r.m[3]) && r.m[3]>1,"U16 finesse");
    std::cout<<"U16 amplitude="<<p.amplitude<<" background="<<p.background
             <<" phase="<<r.m[0]<<" finesse="<<r.m[3]<<'\n';
    return r;
}
void rangesAndEquivalence() {
    synthetic::Parameters p;
    const auto u8=synthetic::frame(p);
    std::vector<unsigned char> crop8(u8.size());
    Result base{};
    check(FP_AnalyzeFrameU8(u8.data(),p.width,p.height,crop8.data(),
          static_cast<int>(crop8.size()),base.roi,base.m)==0,"legacy U8 baseline");
    for (int gain : {1,4,16,64,257}) {
        std::vector<unsigned short> image(u8.size());
        for (std::size_t i=0;i<u8.size();++i) image[i]=static_cast<unsigned short>(u8[i]*gain);
        const auto r=analyze(image,p);
        check(std::abs(r.roi[0]+r.m[1]-base.roi[0]-base.m[1])<0.1 &&
              std::abs(r.roi[1]+r.m[2]-base.roi[1]-base.m[2])<0.1,"equivalent U8/U16 center");
        check(synthetic::phaseError(r.m[0],base.m[0])<0.005,"equivalent U8/U16 phase");
        check(std::abs(r.m[3]/base.m[3]-1)<0.05,"equivalent U8/U16 finesse");
    }
    // Generate directly in U16, retaining information absent from U8 frames.
    for (int bits : {8,10,12,14,16}) {
        p.amplitude=((1u<<bits)-1)*0.80;
        p.background=((1u<<bits)-1)*0.02;
        const auto narrow=analyze(synthetic::frameTyped<unsigned short>(p),p);
        p.airy=5;
        const auto broad=analyze(synthetic::frameTyped<unsigned short>(p),p);
        check(narrow.m[3]>1.7*broad.m[3],"U16 linewidth/finesse response");
        p.airy=24;
    }
    // Every pixel has the same high byte: shifting to U8 destroys the rings.
    p.amplitude=64; p.background=40000;
    auto image=synthetic::frameTyped<unsigned short>(p);
    check(std::all_of(image.begin(),image.end(),[](unsigned short v){return (v>>8)==156;}),"low-bit fixture");
    analyze(image,p);
    // Exercise exact endpoints inside the detected crop, outside useful rings.
    p={}; image=synthetic::frameTyped<unsigned short>(p);
    auto initial=analyze(image,p);
    const auto i=static_cast<std::size_t>(initial.roi[1]+2)*p.width+initial.roi[0]+2;
    image[i]=0; image[i+1]=65535;
    const auto endpoints=analyze(image,p);
    check(initial.roi[0]==endpoints.roi[0] && initial.roi[1]==endpoints.roi[1] &&
          initial.roi[2]==endpoints.roi[2],"endpoint fixture stays in crop");
    // Rectangular, portrait, small localized and phase-wrap cases.
    for (double phase : {0.005,0.995,-0.27}) {
        p.phase=phase; p.amplitude=3000; p.background=500;
        analyze(synthetic::frameTyped<unsigned short>(p),p);
    }
    p={}; p.width=480; p.height=640; p.cx=238.4; p.cy=321.1; p.amplitude=12000;
    analyze(synthetic::frameTyped<unsigned short>(p),p);
    p={}; p.width=1280; p.height=960; p.cx=350.7; p.cy=601.3;
    p.radius=110; p.spacing=1800; p.amplitude=50000;
    analyze(synthetic::frameTyped<unsigned short>(p),p);
}
void capacityAndInvalid() {
    synthetic::Parameters p; p.amplitude=50000; p.background=1000;
    const auto image=synthetic::frameTyped<unsigned short>(p);
    unsigned short tiny[8]; std::fill_n(tiny,8,sentinel);
    Result r{};
    check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,tiny,0,r.roi,r.m)==-2,"zero capacity");
    check(r.roi[2]>0,"capacity reports ROI"); nanMetrics(r.m);
    const int required=r.roi[2]*r.roi[2];
    std::vector<unsigned short> crop(static_cast<std::size_t>(required)+2,sentinel);
    check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data()+1,required-1,r.roi,r.m)==-2,"one element short");
    check(std::all_of(crop.begin(),crop.end(),[](unsigned short v){return v==sentinel;}),"no partial crop");
    check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data()+1,required,r.roi,r.m)==0,"capacity is U16 elements, not bytes");
    check(crop.front()==sentinel && crop.back()==sentinel,"exact capacity guards");
    unsigned short input[64]={};
    for (auto dims : {std::pair<int,int>{0,8},{-1,8},{8,0},{8,-1},
                     {46341,46341},{65536,65536},{INT_MAX,INT_MAX}}) {
        check(FP_AnalyzeFrameU16(input,dims.first,dims.second,tiny,8,r.roi,r.m)==-1,"invalid dimensions");
        check(r.roi[0]==0 && r.roi[1]==0 && r.roi[2]==0,"invalid ROI zero"); nanMetrics(r.m);
    }
    check(FP_AnalyzeFrameU16(nullptr,8,8,tiny,8,r.roi,r.m)==-1,"null input");
    check(FP_AnalyzeFrameU16(input,8,8,nullptr,8,r.roi,r.m)==-1,"null crop");
    check(FP_AnalyzeFrameU16(input,8,8,tiny,8,nullptr,r.m)==-1,"null ROI");
    check(FP_AnalyzeFrameU16(input,8,8,tiny,8,r.roi,nullptr)==-1,"null metrics");
    check(FP_AnalyzeFrameU16(input,8,8,tiny,-1,r.roi,r.m)==-1,"negative capacity");
    r.roi[0]=123; r.m[0]=456;
    check(FP_AnalyzeFrameU16(input,8,8,input+48,8,r.roi,r.m)==-1,"overlap counts bytes, not elements");
    check(r.roi[0]==123 && r.m[0]==456,"overlap leaves outputs untouched");
    check(std::all_of(tiny,tiny+8,[](unsigned short v){return v==sentinel;}),"invalid calls preserve crop");
}
void absentAndFailure() {
    synthetic::Parameters p;
    std::vector<unsigned short> image(static_cast<std::size_t>(p.width)*p.height),crop(image.size(),sentinel);
    Result r{};
    for (unsigned short value : {0,255,4095,40000,65535}) {
        std::fill(image.begin(),image.end(),value);
        check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),r.roi,r.m)==-3,"constant U16 image");
        check(r.roi[2]==0,"absent ROI"); nanMetrics(r.m);
    }
    image[image.size()/2]=0;
    check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),r.roi,r.m)==-3,"single extreme outlier");
    p.amplitude=40000; p.background=1000; p.spacing=100000;
    image=synthetic::frameTyped<unsigned short>(p);
    check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),r.roi,r.m)==-4,"insufficient U16 rings");
    nanMetrics(r.m); check(r.roi[2]>0,"diagnostic U16 crop valid");
    for (int y=0;y<r.roi[2];++y) for (int x=0;x<r.roi[2];++x)
        check(crop[static_cast<std::size_t>(y)*r.roi[2]+x]==image[static_cast<std::size_t>(y+r.roi[1])*p.width+x+r.roi[0]],"failure exact U16 crop");
    for (unsigned seed=1;seed<=4;++seed) {
        p={}; p.amplitude=0; p.background=30000; p.noise=800; p.seed=seed;
        image=synthetic::frameTyped<unsigned short>(p);
        check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),r.roi,r.m)<0,"U16 noise rejected");
        nanMetrics(r.m);
    }
}
}
int main() {
    int failed=0;
    const std::pair<const char*,void(*)()> tests[]={
        {"U16 ranges, precision and U8 equivalence",rangesAndEquivalence},
        {"U16 capacity, guards and invalid arguments",capacityAndInvalid},
        {"U16 absent and insufficient rings",absentAndFailure}};
    for (const auto& test:tests) {
        try { test.second(); std::cout<<"PASS "<<test.first<<'\n'; }
        catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<test.first<<": "<<e.what()<<'\n'; }
    }
    std::cout<<"U16 groups passed="<<3-failed<<" failed="<<failed<<'\n';
    return failed?1:0;
}
