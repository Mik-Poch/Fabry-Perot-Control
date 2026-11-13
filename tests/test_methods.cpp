extern "C" {
#include "FPAnalyzer.h"
}
#include "Pipeline.h"
#include "Synthetic.h"
#include <array>
#include <cmath>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
struct Result {int status;std::array<int,3> roi;std::array<double,4> metrics;};
Result run(const std::vector<unsigned short>& image,int width,int height,const int* selected,int method) {
    const int capacity=std::min(width,height)*std::min(width,height);
    std::vector<unsigned short> crop(capacity+2,0xA55A);
    struct {int before=123;int values[3]={};int after=456;} roi;
    struct {double before=123;double values[4]={};double after=456;} m;
    Result r{};
    r.status=FP_AnalyzeFrameU16WithMethod(image.data(),width,height,selected,method,crop.data()+1,capacity,roi.values,m.values);
    std::copy_n(roi.values,3,r.roi.begin());std::copy_n(m.values,4,r.metrics.begin());
    check(roi.before==123 && roi.after==456 && m.before==123 && m.after==456,"metadata length guards");
    check(crop.front()==0xA55A && crop.back()==0xA55A,"crop capacity guards");
    if(r.status==0 || (r.status==-4 && r.roi[2]>0)) {
        for(int y=0;y<r.roi[2];++y)for(int x=0;x<r.roi[2];++x)
            check(crop[1+std::size_t(y)*r.roi[2]+x]==image[std::size_t(y+r.roi[1])*width+x+r.roi[0]],"all methods preserve exact U16");
        for(std::size_t i=1+std::size_t(r.roi[2])*r.roi[2];i<crop.size();++i)check(crop[i]==0xA55A,"crop tail guard");
    }
    if(r.status!=0)for(double value:r.metrics)check(std::isnan(value),"failure metrics always NaN, no fallback");
    return r;
}
void baseline() {
    // Recorded from source a9a0316 BEFORE refactoring/optimizing, noise=0.
    const double expected[6][4]={
        {.26860188652512273,229.30076831227782,219.69923168772405,7.9435832977507461},
        {.99497493045234897,229.30091255463537,219.69908744536522,7.9482544651713019},
        {.0049730449383845743,229.30002791884371,219.69997208115805,7.950125369571083},
        {.26865524248087924,190.40064636501413,190.19950452800646,7.9455617272198973},
        {.27143411353937597,229.29971413921584,219.70028586078288,8.0001188976465372},
        {.72746129822028283,229.30025684856929,219.69974315142844,4.0464659544919686}};
    for(int k=0;k<6;++k) {
        synthetic::Parameters p;p.amplitude=50000;p.background=1000;
        if(k==1)p.phase=.995;if(k==2)p.phase=.005;
        if(k==3){p.cx=280.4;p.cy=210.2;}if(k==4){p.amplitude=64;p.background=40000;}
        if(k==5){p.airy=5;p.phase=.73;}
        const auto image=synthetic::frame(p);const int selected[3]={90,20,420};
        const auto r=run(image,p.width,p.height,selected,1);
        check(r.status==0,"reference fixture succeeds");
        // Same numerical tolerances as the prior U16 equivalence checks;
        // libm/compiler differences must not turn this into a bitwise golden.
        const double tolerance[4]={.005,.1,.1,.05*expected[k][3]};
        for(int i=0;i<4;++i)
            check(std::abs(r.metrics[i]-expected[k][i])<tolerance[i],"v2 method B golden regression");
        std::vector<unsigned short> crop(image.size());int roi[3];double m[4];
        check(FP_AnalyzeFrameU16(image.data(),p.width,p.height,selected,crop.data(),int(crop.size()),roi,m)==r.status,"legacy status");
        for(int i=0;i<4;++i)check(m[i]==r.metrics[i],"original API exactly delegates method B");
    }
}
synthetic::Parameters fixture(){synthetic::Parameters p;p.width=384;p.height=320;p.cx=185.35;p.cy=162.7;p.radius=125;p.spacing=2200;p.phase=.73;p.airy=20;p.amplitude=50000;p.background=1000;return p;}
void comparisons() {
    const int selected[3]={45,25,280};
    for(int k=0;k<19;++k) {
        auto p=fixture();bool required=k<13;
        std::string name="clean";
        if(k==1){p.phase=.005;name="wrap_low";}if(k==2){p.phase=.995;name="wrap_high";}
        if(k==3){p.cx-=24;p.cy+=23;name="offset_center";}
        if(k==4){p.airy=5;name="broad";}if(k==5){p.airy=40;name="narrow";}
        if(k==6){p.noise=250;name="noise_0.5pct";}if(k==7){p.noise=500;name="noise_1pct";}
        if(k==8){p.backgroundX=3000;p.backgroundY=-1500;name="background_plane";}
        if(k==9){p.illuminationX=.5;p.illuminationY=-.3;name="illumination";}
        if(k==10){p.amplitude=200;p.background=8;name="8bit";}
        if(k==11){p.amplitude=800;p.background=20;name="10bit";}
        if(k==12){p.amplitude=3200;p.background=80;name="12bit";}
        if(k==13){p.amplitude=64;p.background=40000;name="low_bits";}
        if(k==14){p.noise=1500;name="noise_3pct";}
        if(k==15){p.noise=5000;name="noise_10pct";}
        if(k==16){p.ellipticity=.015;name="ellipse_1.5pct";}
        if(k==17){p.ellipticity=.05;name="ellipse_5pct";}
        if(k==18){p.amplitude=500;p.noise=300;name="low_snr";}
        const auto image=synthetic::frame(p);
        const double truthFinesse=synthetic::pi/(2*std::asin(1/std::sqrt(p.airy+2)));
        for(int method=0;method<4;++method) {
            const auto r=run(image,p.width,p.height,selected,method);
            std::cout<<"ACCURACY,"<<name<<','<<method<<','<<r.status;
            if(r.status==0) {
                const double ex=r.metrics[1]+selected[0]-p.cx,ey=r.metrics[2]+selected[1]-p.cy;
                const double ep=synthetic::phaseError(r.metrics[0],p.phase),ef=r.metrics[3]/truthFinesse-1;
                std::cout<<','<<ex<<','<<ey<<','<<ep<<','<<ef;
                check(r.metrics[0]>=0 && r.metrics[0]<1 && r.metrics[3]>1,"physical outputs");
                if(required)check(std::abs(ex)<.7 && std::abs(ey)<.7 && ep<.03 && std::abs(ef)<.25,"accuracy "+name+" method "+std::to_string(method));
            }
            std::cout<<'\n';
            if(required)check(r.status==0,"required fixture "+name+" method "+std::to_string(method));
        }
    }
}
void contracts() {
    auto p=fixture();const auto image=synthetic::frame(p);const int selected[3]={45,25,280},automatic[3]={0,0,0};
    for(int method=0;method<4;++method) {
        const auto a=run(image,p.width,p.height,automatic,method);
        check(a.status==0,"all automatic methods");
        const auto b=run(image,p.width,p.height,a.roi.data(),method);
        check(a.metrics==b.metrics,"auto/manual equivalent metrics");
        auto altered=image;
        for(int y=0;y<p.height;++y)for(int x=0;x<p.width;++x)
            if(x<selected[0] || x>=selected[0]+selected[2] || y<selected[1] || y>=selected[1]+selected[2])
                altered[std::size_t(y)*p.width+x]=((x+y)&1)?65535:0;
        check(run(image,p.width,p.height,selected,method).metrics==run(altered,p.width,p.height,selected,method).metrics,
              "all manual methods ignore exterior statistics");
        std::vector<unsigned short> crop(280*280+2,0xA55A);int roi[3];double m[4];
        check(FP_AnalyzeFrameU16WithMethod(image.data(),p.width,p.height,selected,method,crop.data()+1,280*280-1,roi,m)==-2,"all method short capacity");
        check(std::all_of(crop.begin(),crop.end(),[](unsigned short v){return v==0xA55A;}),"no short-capacity writes");
        const int bad[3]={-1,0,10};
        check(FP_AnalyzeFrameU16WithMethod(image.data(),p.width,p.height,bad,method,crop.data()+1,280*280,roi,m)==-1,"invalid ROI precedes analysis");
        auto blank=image;std::fill(blank.begin(),blank.end(),1234);
        check(run(blank,p.width,p.height,selected,method).status==-4,"blank manual rejected");
        for(unsigned seed=1;seed<=3;++seed) {
            auto noise=p;noise.amplitude=0;noise.background=30000;noise.noise=1000;noise.seed=seed;
            check(run(synthetic::frame(noise),p.width,p.height,selected,method).status==-4,"pure noise rejected");
        }
    }
    for(int method : {-1,4,2147483647}) {
        const auto r=run(image,p.width,p.height,selected,method);
        check(r.status==FP_INVALID_METHOD && r.roi==std::array<int,3>{0,0,0},"invalid method contract");
    }
    std::vector<std::future<void>> workers;
    std::array<Result,4> sequential;
    for(int m=0;m<4;++m)sequential[m]=run(image,p.width,p.height,selected,m);
    for(int m=0;m<4;++m)workers.push_back(std::async(std::launch::async,[&,m]{
        for(int i=0;i<3;++i){const auto r=run(image,p.width,p.height,selected,m);check(r.status==0 && r.metrics==sequential[m].metrics,"concurrent deterministic calls");}
    }));
    for(auto& w:workers)w.get();
}
}
int main(){try{std::cout<<std::setprecision(10);baseline();comparisons();contracts();std::cout<<"PASS method baseline, comparison, contracts and concurrency\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
