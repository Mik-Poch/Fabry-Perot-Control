extern "C" {
#include "FPAnalyzer.h"
}
#include "Pipeline.h"
#include "Synthetic.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
struct Timing {std::vector<double> ms;int passed=0,failed=0;};
double percentile(std::vector<double> v,double q){if(v.empty())return 0;std::sort(v.begin(),v.end());return v[std::min(v.size()-1,std::size_t(std::ceil(q*v.size())-1))];}
}
int main(int argc,char** argv){try{
    const int iterations=argc>1?std::stoi(argv[1]):21;
    if(iterations<3 || iterations>10000)throw std::runtime_error("iterations must be 3..10000");
    std::ofstream file;
    if(argc>2){file.open(argv[2]);if(!file)throw std::runtime_error("cannot open benchmark report");}
    auto report=[&](const std::string& line){std::cout<<line<<'\n';if(file)file<<line<<'\n';};
    report("kind,pointer_bits,radius,roi_size,method,mode,iterations,success,failure,median_ms,p95_ms,min_ms,max_ms");
    for(int radius:{110,340}) {
        synthetic::Parameters p;p.width=1280;p.height=960;p.cx=645.4;p.cy=470.8;
        p.radius=radius;p.spacing=radius==110?1800:15000;p.amplitude=50000;p.background=1000;p.noise=3;
        const auto image=synthetic::frame(p);
        std::vector<unsigned short> crop(960*960);
        const int automatic[3]={0,0,0};int selected[3],roi[3];double m[4];
        if(FP_AnalyzeFrameU16(image.data(),p.width,p.height,automatic,crop.data(),int(crop.size()),selected,m)!=0)
            throw std::runtime_error("benchmark ROI fixture failed");
        for(int method=0;method<4;++method) {
            for(bool autoMode:{false,true}) {
                const int* inputRoi=autoMode?automatic:selected;
                Timing t;
                for(int i=-2;i<iterations;++i) {
                    const auto begin=Clock::now();
                    const int status=FP_AnalyzeFrameU16WithMethod(image.data(),p.width,p.height,inputRoi,method,crop.data(),int(crop.size()),roi,m);
                    const auto end=Clock::now();
                    if(i<0)continue;
                    t.ms.push_back(std::chrono::duration<double,std::milli>(end-begin).count());
                    if(status==0){
                        ++t.passed;
                        if(!std::equal(roi,roi+3,selected))throw std::runtime_error("different benchmark ROI");
                        const double truthF=synthetic::pi/(2*std::asin(1/std::sqrt(p.airy+2)));
                        if(std::abs(roi[0]+m[1]-p.cx)>1 || std::abs(roi[1]+m[2]-p.cy)>1 ||
                           synthetic::phaseError(m[0],p.phase)>.03 || std::abs(m[3]/truthF-1)>.25)
                            throw std::runtime_error("benchmark four-metric accuracy failure");
                    }
                    else ++t.failed;
                }
                std::ostringstream line;line<<std::setprecision(8)<<"dll,"<<8*sizeof(void*)<<','<<radius<<','<<selected[2]<<','<<method<<','<<(autoMode?"auto":"manual")<<','<<iterations<<','<<t.passed<<','<<t.failed<<','<<percentile(t.ms,.5)<<','<<percentile(t.ms,.95)<<','<<*std::min_element(t.ms.begin(),t.ms.end())<<','<<*std::max_element(t.ms.begin(),t.ms.end());
                report(line.str());
                if(t.failed)throw std::runtime_error("benchmark fixture failed for method "+std::to_string(method));
                // Separate instrumented common implementation calls. Do not add
                // stage timings to the DLL timer or claim these are LV timings.
                std::array<Timing,7> stages;
                for(int i=-1;i<iterations;++i) {
                    fp::Diagnostics d;
                    if(fp::analyzeFrame(image.data(),p.width,p.height,inputRoi,method,crop.data(),int(crop.size()),roi,m,&d)!=0)throw std::runtime_error("instrumented benchmark failure");
                    const double values[]={d.roiMs,d.copyMs,d.normalizationMs,d.centerMs,d.profileMs,d.peaksMs,d.fitMs};
                    if(i>=0)for(int s=0;s<7;++s)stages[s].ms.push_back(values[s]);
                }
                const char* names[]={"roi_with_frame_normalization","copy","crop_normalization","center","profile","peaks","nonlinear_or_sector_fit"};
                for(int s=0;s<7;++s) {
                    std::ostringstream stage;stage<<std::setprecision(8)<<names[s]<<','<<8*sizeof(void*)<<','<<radius<<','<<selected[2]<<','<<method<<','<<(autoMode?"auto":"manual")<<','<<iterations<<','<<iterations<<",0,"<<percentile(stages[s].ms,.5)<<','<<percentile(stages[s].ms,.95)<<','<<*std::min_element(stages[s].ms.begin(),stages[s].ms.end())<<','<<*std::max_element(stages[s].ms.begin(),stages[s].ms.end());report(stage.str());
                }
            }
        }
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
