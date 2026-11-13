extern "C" {
#include "FPAnalyzer.h"
}
#include "Synthetic.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace {
using Analyze=int(*)(const unsigned short*,int,int,const int*,unsigned short*,int,int*,double*);
struct PreviousLibrary {
#ifdef _WIN32
    HMODULE handle;
    explicit PreviousLibrary(const char* path):handle(LoadLibraryA(path)){}
    ~PreviousLibrary(){if(handle)FreeLibrary(handle);}
    Analyze function(){return handle?reinterpret_cast<Analyze>(GetProcAddress(handle,"FP_AnalyzeFrameU16")):nullptr;}
#else
    void* handle;
    explicit PreviousLibrary(const char* path):handle(dlopen(path,RTLD_NOW|RTLD_LOCAL)){}
    ~PreviousLibrary(){if(handle)dlclose(handle);}
    Analyze function(){return handle?reinterpret_cast<Analyze>(dlsym(handle,"FP_AnalyzeFrameU16")):nullptr;}
#endif
};
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: fp_b_compare /absolute/path/to/previous/FPAnalyzer.dll-or-so");
    PreviousLibrary library(argv[1]);const auto old=library.function();
    if(!old)throw std::runtime_error("cannot load same-architecture previous library with original U16 signature");
    std::cout<<"radius,roi_size,mode,iterations,old_median_ms,new_median_ms,old_p95_ms,new_p95_ms,reduction_percent\n";
    for(int radius:{110,340}) {
        synthetic::Parameters p;p.width=1280;p.height=960;p.cx=645.4;p.cy=470.8;
        p.radius=radius;p.spacing=radius==110?1800:15000;p.amplitude=50000;p.background=1000;p.noise=3;
        const auto image=synthetic::frame(p);std::vector<unsigned short> crop(960*960);
        const int automatic[3]={0,0,0};int selected[3],roi[3];double metrics[4];
        if(old(image.data(),p.width,p.height,automatic,crop.data(),int(crop.size()),selected,metrics)!=0)
            throw std::runtime_error("previous library cannot analyze fixture");
        for(bool automaticMode:{false,true}) {
            std::vector<double> times[2];const int* in=automaticMode?automatic:selected;
            for(int i=-2;i<25;++i)for(int j=0;j<2;++j) {
                const int version=(j+(i&1))%2;
                const auto begin=std::chrono::steady_clock::now();
                const int status=(version?FP_AnalyzeFrameU16:old)(image.data(),p.width,p.height,in,crop.data(),int(crop.size()),roi,metrics);
                const auto end=std::chrono::steady_clock::now();
                if(status!=0 || !std::equal(roi,roi+3,selected))throw std::runtime_error("analysis failed or ROI differs");
                if(i>=0)times[version].push_back(std::chrono::duration<double,std::milli>(end-begin).count());
            }
            for(auto& t:times)std::sort(t.begin(),t.end());
            std::cout<<radius<<','<<selected[2]<<','<<(automaticMode?"auto":"manual")<<",25,"<<times[0][12]<<','<<times[1][12]<<','<<times[0][23]<<','<<times[1][23]<<','<<100*(1-times[1][12]/times[0][12])<<'\n';
        }
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
