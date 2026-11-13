extern "C" {
#include "FPAnalyzer.h"
}
#include "Synthetic.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    try {
        for (int radius : {110,340}) {
            synthetic::Parameters p;
            p.width=1280; p.height=960; p.cx=645.4; p.cy=470.8;
            p.radius=radius; p.spacing=radius==110?1800:15000;
            p.amplitude=50000; p.background=1000; p.noise=3;
            const auto image=synthetic::frame(p);
            std::vector<unsigned short> crop(960*960);
            const int automatic[3]={0,0,0};
            int selected[3], roi[3]; double metrics[4];
            if (FP_AnalyzeFrameU16(image.data(),p.width,p.height,automatic,crop.data(),
                    static_cast<int>(crop.size()),selected,metrics)!=0)
                throw std::runtime_error("benchmark auto fixture failed");
            const auto call = [&](const int* inputRoi) {
                const auto start=std::chrono::steady_clock::now();
                const int status=FP_AnalyzeFrameU16(image.data(),p.width,p.height,inputRoi,
                    crop.data(),static_cast<int>(crop.size()),roi,metrics);
                const auto stop=std::chrono::steady_clock::now();
                if (status!=0 || !std::equal(roi,roi+3,selected))
                    throw std::runtime_error("benchmark measurement/geometry failed");
                return std::chrono::duration<double,std::milli>(stop-start).count();
            };
            call(selected); // Both modes warm before timing; alternate order.
            std::vector<double> autoMs,manualMs;
            constexpr int repeats=9;
            for (int i=0;i<repeats;++i) {
                if (i%2) { manualMs.push_back(call(selected)); autoMs.push_back(call(automatic)); }
                else { autoMs.push_back(call(automatic)); manualMs.push_back(call(selected)); }
            }
            std::sort(autoMs.begin(),autoMs.end()); std::sort(manualMs.begin(),manualMs.end());
            const double a=autoMs[repeats/2],m=manualMs[repeats/2];
            std::cout<<std::fixed<<std::setprecision(3)<<"BENCHMARK U16 1280x960 radius="<<radius
                     <<" ROI="<<selected[0]<<','<<selected[1]<<','<<selected[2]
                     <<" repeats="<<repeats<<" auto_median_ms="<<a<<" manual_median_ms="<<m
                     <<" speedup="<<a/m<<" reduction_percent="<<100*(1-m/a)<<'\n';
        }
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
