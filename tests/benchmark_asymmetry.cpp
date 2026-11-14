extern "C" {
#include "FPAnalyzer.h"
}
#include "AsymmetrySynthetic.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    std::ofstream file;
    if(argc>1) {file.open(argv[1]);if(!file) return 1;}
    std::ostream& out=argc>1 ? file : std::cout;
    out<<"pointer_bits,frame_size,roi_size,mode,repetitions,failures,median_ms,p95_ms\n";
    int failures=0;
    for(int size:{256,320,640}) {
        asymSynthetic::Parameters p;p.size=size;p.cx=size*.48;p.cy=size*.51;
        p.spacing=3600.*size*size/(320.*320.);p.ax=.2;p.wy=.12;p.aperture=size*.46;
        auto image=asymSynthetic::frame(p);
        int roi[3]={},selected[3]={};double m[16],s[64];
        if(FP_AnalyzeFringeAsymmetryU16(image.data(),size,size,selected,roi,m,s)!=0) return 2;
        const int selectedSize=roi[2];
        for(bool automatic:{false,true}) {
            const int input[]={automatic?0:roi[0],automatic?0:roi[1],automatic?0:roi[2]};
            std::vector<double> times;int failed=0;
            for(int i=-3;i<31;++i) {
                const auto begin=std::chrono::steady_clock::now();
                const int status=FP_AnalyzeFringeAsymmetryU16(image.data(),size,size,input,selected,m,s);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                if(i>=0) {times.push_back(ms);if(status!=0) ++failed;}
                else if(status!=0) return 3;
            }
            std::sort(times.begin(),times.end());failures+=failed;
            out<<sizeof(void*)*8<<','<<size<<','<<selectedSize<<','<<(automatic?"auto":"manual")<<",31,"<<failed<<','<<times[15]<<','<<times[29]<<'\n';
        }
    }
    return failures ? 4 : 0;
}
