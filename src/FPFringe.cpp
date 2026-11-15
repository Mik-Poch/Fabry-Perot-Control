extern "C" {
#include "FPFringe.h"
}
#include "Analysis.h"
#include "Methods.h"
#include "Asymmetry.h"
#include "FringeDetection.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <limits>

namespace {
constexpr double missing=std::numeric_limits<double>::quiet_NaN();
static_assert(sizeof(int)==4 && sizeof(unsigned short)==2 && sizeof(double)==8 && CHAR_BIT==8,"I32/U16/DBL ABI required");
struct Buffer {const void* pointer;std::size_t bytes;};
bool disjoint(std::initializer_list<Buffer> buffers) {
    for(auto a=buffers.begin();a!=buffers.end();++a) for(auto b=a+1;b!=buffers.end();++b) {
        if(!a->pointer || !b->pointer || !a->bytes || !b->bytes) continue;
        const auto x=reinterpret_cast<std::uintptr_t>(a->pointer),y=reinterpret_cast<std::uintptr_t>(b->pointer);
        if(x<=y ? y-x<a->bytes : x-y<b->bytes) return false;
    }
    return true;
}
std::size_t count(int width,int height) {
    const auto n=width>0 && height>0 ? static_cast<std::uint64_t>(width)*height : 0;
    return n>0 && n<=INT_MAX ? static_cast<std::size_t>(n) : 0;
}
void invalidSectors(double* s) {if(s) for(int i=0;i<16;++i) {s[4*i]=s[4*i+1]=missing;s[4*i+2]=s[4*i+3]=0;}}
}

extern "C" FPF_INTERNAL_EXPORT int FPF_INTERNAL_CALL FPF_GetApiVersion(void) {return 3;}

extern "C" FPF_INTERNAL_EXPORT int FPF_INTERNAL_CALL FPF_FindCenteredRoiU16(
    const unsigned short* pixels,int width,int height,int requested,
    unsigned short* crop,int capacity,int* roi) {
    const auto n=count(width,height);
    // A non-null zero-capacity pointer must still be a disjoint one-element
    // placeholder. Check storage before writes, including partial-null calls.
    const auto cropBytes=static_cast<std::size_t>(std::max(1,capacity))*sizeof(unsigned short);
    if(!disjoint({{pixels,std::max<std::size_t>(1,n)*sizeof(unsigned short)},
                  {crop,cropBytes},{roi,3*sizeof(int)}})) return -1;
    if(roi) std::fill_n(roi,3,0);
    if(!pixels || !crop || !roi || !n || capacity<0 || requested<0 || requested>width || requested>height) return -1;
    try {
        fp::Roi located;
        const int status=fp::findCenteredRoi(fp::analysisView(pixels,n),width,height,requested,located);
        if(status) return status;
        roi[0]=located.left;roi[1]=located.top;roi[2]=located.size;
        const auto required=static_cast<std::size_t>(located.size)*located.size;
        if(required>static_cast<std::size_t>(capacity)) return -2;
        // No allocation/analysis remains after the first output-pixel write.
        for(int y=0;y<located.size;++y)
            std::memcpy(crop+static_cast<std::size_t>(y)*located.size,
                        pixels+static_cast<std::size_t>(y+located.top)*width+located.left,
                        static_cast<std::size_t>(located.size)*sizeof(unsigned short));
        return 0;
    } catch(...) {
        std::fill_n(roi,3,0);
        return -4;
    }
}

extern "C" FPF_INTERNAL_EXPORT int FPF_INTERNAL_CALL FPF_AnalyzeU16(
    const unsigned short* pixels,int width,int height,int method,double* metrics) {
    const auto n=count(width,height);
    if(!disjoint({{pixels,n*2},{metrics,4*sizeof(double)}})) return -1;
    if(metrics) std::fill_n(metrics,4,missing);
    if(!pixels || !metrics || !n) return -1;
    if(method<0 || method>3) return -5;
    try {
        fp::Center center;fp::Measurement result;
        if(!fp::analyzeImage(fp::analysisView(pixels,n),width,height,method,center,result)) return -4;
        metrics[0]=result.phase;metrics[1]=center.x;metrics[2]=center.y;metrics[3]=result.finesse;
        return 0;
    } catch(...) {return -4;}
}

extern "C" FPF_INTERNAL_EXPORT int FPF_INTERNAL_CALL FPF_AnalyzeAsymmetryU16(
    const unsigned short* pixels,int width,int height,double* summary,double* sectors) {
    const auto n=count(width,height);
    if(!disjoint({{pixels,n*2},{summary,16*sizeof(double)},{sectors,64*sizeof(double)}})) return -1;
    if(summary) std::fill_n(summary,16,missing);
    invalidSectors(sectors);
    if(!pixels || !summary || !sectors || !n) return -1;
    try {return fp::measureAsymmetry(fp::analysisView(pixels,n),width,height,summary,sectors) ? 0 : -4;}
    catch(...) {return -4;}
}
