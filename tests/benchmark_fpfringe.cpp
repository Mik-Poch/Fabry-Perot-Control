extern "C" {
#include "FPFringe.h"
}
#include "Synthetic.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    std::ofstream file;
    if(argc>1) {file.open(argv[1]);if(!file) return 1;}
    std::ostream& out=argc>1 ? file : std::cout;
    out<<"pointer_bits,build,width,height,operation,repetitions,successes,failures,median_ms,p95_ms\n";
    int failures=0;
    for(auto shape:std::vector<std::pair<int,int>>{{320,320},{640,480},{1280,960}}) {
        synthetic::Parameters p;p.width=shape.first;p.height=shape.second;
        p.cx=(p.width-1)/2.+.3;p.cy=(p.height-1)/2.-.2;const double side=std::min(p.width,p.height);
        p.radius=side*.46;p.spacing=side*side*.035;p.airy=5;p.amplitude=30000;p.background=1000;
        const auto image=synthetic::frame(p);
        int roi[4];double center[2],m[4],a[16],s[64];
        for(int operation=0;operation<6;++operation) {
            const std::string name=operation==0?"ROI":operation==5?"asymmetry":std::string("ABCD").substr(operation-1,1);
            std::vector<double> times;int failed=0;
            for(int k=-2;k<21;++k) {
                const auto start=std::chrono::steady_clock::now();int status;
                if(operation==0) status=FPF_FindCenteredRoiU16(image.data(),p.width,p.height,0,roi,center);
                else if(operation==5) status=FPF_AnalyzeAsymmetryU16(image.data(),p.width,p.height,a,s);
                else status=FPF_AnalyzeU16(image.data(),p.width,p.height,operation-1,m);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(k>=0) {times.push_back(ms);failed+=status!=0;} else if(status) return 2;
            }
            std::sort(times.begin(),times.end());failures+=failed;
            out<<sizeof(void*)*8<<','<<FPF_BENCHMARK_CONFIG<<','<<p.width<<','<<p.height<<','<<name<<",21,"<<21-failed<<','<<failed<<','<<times[10]<<','<<times[19]<<'\n';
        }
    }
    return failures ? 3 : 0;
}
