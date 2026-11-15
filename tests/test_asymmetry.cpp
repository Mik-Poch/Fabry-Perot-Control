extern "C" {
#include "FPAnalyzer.h"
}
#include "AsymmetrySynthetic.h"
#include <future>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <climits>
struct Result { int status; int roi[3]; double m[16],s[64]; };
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
Result run(const asymSynthetic::Parameters& p,bool automatic=false) {
    auto image=asymSynthetic::frame(p),original=image;
    const int roi[]={0,0,automatic ? 0 : p.size};
    struct Guard { double before=123, m[16]{},middle=456,s[64]{},after=789; } g;
    struct RoiGuard {int before=123,roi[3]{},after=456;} r;
    Result result{};
    result.status=FP_AnalyzeFringeAsymmetryU16(image.data(),p.size,p.size,roi,r.roi,g.m,g.s);
    require(g.before==123 && g.middle==456 && g.after==789 && r.before==123 && r.after==456,"output canaries");
    require(image==original,"source unchanged");
    std::copy_n(r.roi,3,result.roi); std::copy_n(g.m,16,result.m);std::copy_n(g.s,64,result.s);
    return result;
}
void check(const asymSynthetic::Parameters& p,const std::string& name,double tolerance=.04) {
    const auto r=run(p);
    std::cout<<name<<",status="<<r.status<<",Ax="<<r.m[4]<<",Ay="<<r.m[5]<<",Wx="<<r.m[6]<<",Wy="<<r.m[7]<<",rings="<<r.m[13]<<",sectors="<<r.m[14]<<",center_error="<<std::hypot(r.m[1]-p.cx,r.m[2]-p.cy)<<",rms="<<r.m[15]<<'\n';
    require(r.status==0,name+" success");
    const double expected[]={p.ax,p.ay,p.wx,p.wy};
    for(int i=0;i<4;++i) require(std::abs(r.m[4+i]-expected[i])<tolerance,name+" signed coefficient "+std::to_string(i));
    require(std::hypot(r.m[1]-p.cx,r.m[2]-p.cy)<.65,name+" center");
    require(synthetic::phaseError(r.m[0],p.phase)<.035,name+" phase");
    require(r.m[13]>=2 && r.m[14]>=10,name+" reliability");
    for(int s=0;s<16;++s) {
        if(r.s[4*s+3]==1) require(r.s[4*s]>0 && r.s[4*s+1]>0 && r.s[4*s+2]>=2,name+" valid sector");
        else require(std::isnan(r.s[4*s]) && std::isnan(r.s[4*s+1]) && r.s[4*s+2]==0,name+" invalid sector");
    }
}
int main() {try {
    asymSynthetic::Parameters p;
    check(p,"symmetric",.025);
    p.cx=132.4;p.cy=181.2;check(p,"displaced",.03);
    p={};p.ax=.3;p.ay=-.2;check(p,"brightness");
    p={};p.wx=-.22;p.wy=.18;check(p,"width");
    p.ax=.3;p.ay=-.2;check(p,"combined-opposite");
    require(run(p).m[12]<-.85,"opposite correlation");
    p.wx=.15;p.wy=.12;check(p,"combined-independent");
    for(double phase:{.05,.27,.53,.85}) {p.phase=phase;check(p,"phase-"+std::to_string(phase));}
    for(int bits:{8,10,12,14,16}) {p.bits=bits;p.scale=.45;p.background=.15;p.noise=((1<<bits)-1)*.002;check(p,"bits-"+std::to_string(bits));}
    p={};p.illuminationX=.8;p.illuminationY=-.5;p.backgroundX=.05;
    auto gradient=run(p);
    require(gradient.status==0 && gradient.m[4]>.1 && gradient.m[5]<-.05 && std::hypot(gradient.m[6],gradient.m[7])<.025,"spatial amplitude gradient without false linewidth");
    std::cout<<"spatial-gradient,Ax="<<gradient.m[4]<<",Ay="<<gradient.m[5]<<",Wx="<<gradient.m[6]<<",Wy="<<gradient.m[7]<<'\n';
    for(auto center:std::vector<std::array<double,2>>{{140.2,139.7},{180.6,152.1},{164.8,183.2}}) {p={};p.cx=center[0];p.cy=center[1];check(p,"center-sweep",.025);}
    p={};p.ax=-.21;p.ay=.28;p.wx=.16;p.wy=-.18;check(p,"reversed-directions");
    p={};p.ax=.2;p.ay=.1;p.obscure=.3;check(p,"missing-sectors",.04);
    p={};p.size=256;p.cx=125.3;p.cy=130.7;p.spacing=2500;check(p,"small-roi",.035);
    p={};p.size=480;p.cx=231.3;p.cy=245.7;p.spacing=6500;check(p,"large-roi",.035);
    p={};p.aperture=140;check(p,"weak-outer",.035);
    p={};p.ax=.3;p.scale=.8;check(p,"mild-saturation",.05);
    p={};p.ellipse=.025;
    auto distorted=run(p);require(distorted.status==0 || distorted.status==-4,"distortion status");
    std::cout<<"ellipse,status="<<distorted.status<<",rms="<<distorted.m[15]<<" (first harmonic cannot diagnose ellipticity)\n";
    p={};auto autoResult=run(p,true);require(autoResult.status==0,"auto ROI");
    // A selected crop must be independent of arbitrary data outside that crop.
    p={};auto originalImage=asymSynthetic::frame(p);auto originalResult=run(p);
    std::vector<unsigned short> padded(384*360,65535);
    for(int y=0;y<p.size;++y) std::copy_n(originalImage.data()+y*p.size,p.size,padded.data()+(y+17)*384+23);
    const int selectedRoi[]={23,17,320};Result paddedResult{};
    paddedResult.status=FP_AnalyzeFringeAsymmetryU16(padded.data(),384,360,selectedRoi,paddedResult.roi,paddedResult.m,paddedResult.s);
    require(paddedResult.status==0 && std::equal(selectedRoi,selectedRoi+3,paddedResult.roi),"exact nonzero manual ROI");
    for(int i=0;i<16;++i) require(paddedResult.m[i]==originalResult.m[i] || (std::isnan(paddedResult.m[i]) && std::isnan(originalResult.m[i])),"manual crop independent of full-frame statistics");
    p.aperture=108;auto twoRings=run(p);
    std::cout<<"limited-aperture,status="<<twoRings.status<<",rings="<<twoRings.m[13]<<'\n';
    require(twoRings.status==0 && twoRings.m[13]>=2,"limited visible rings");
    p={};p.ellipse=.2;require(run(p).status==-4,"severe distortion rejected");
    p={};p.scale=.00001;require(run(p).status==-4,"low contrast rejected");
    p={};p.spacing=30000;require(run(p).status==-4,"too few rings");
    p={};p.obscure=2;require(run(p).status==-4,"insufficient angular coverage");
    unsigned short blank[32*32]={};int manual[]={0,0,32},automatic[]={0,0,0},roi[3];double m[16],s[64];
    auto call=[&](const unsigned short* image,int w,int h,const int* in,int* ro,double* mo,double* so){return FP_AnalyzeFringeAsymmetryU16(image,w,h,in,ro,mo,so);};
    require(call(blank,32,32,automatic,roi,m,s)==-3,"blank auto");
    require(call(blank,32,32,manual,roi,m,s)==-4 && roi[2]==32,"blank manual");
    for(double x:m) require(std::isnan(x),"failure NaN");
    for(int i=0;i<16;++i) require(std::isnan(s[4*i]) && std::isnan(s[4*i+1]) && s[4*i+2]==0 && s[4*i+3]==0,"failure sector");
    for(int which=0;which<5;++which) require(call(which==0?nullptr:blank,32,32,which==1?nullptr:manual,which==2?nullptr:roi,which==3?nullptr:m,which==4?nullptr:s)==-1,"null pointer");
    require(call(blank,INT_MAX,INT_MAX,manual,roi,m,s)==-1,"overflow");
    for(auto bad:std::vector<std::array<int,3>>{{-1,0,24},{0,-1,24},{0,0,-1},{1,0,32},{INT_MAX,0,24},{0,0,INT_MAX},{0,1,0}})
        require(call(blank,32,32,bad.data(),roi,m,s)==-1,"bad ROI");
    require(call(blank,32,32,manual,roi,m,m)==-1,"overlap outputs");
    require(call(blank,32,32,manual,manual,m,s)==-1,"overlap ROI");
    p={};p.ax=.24;p.wy=.16;
    auto reference=run(p);std::vector<std::future<Result>> jobs;
    for(int i=0;i<8;++i) jobs.push_back(std::async(std::launch::async,[p]{return run(p);}));
    for(auto& job:jobs) {auto r=job.get();require(r.status==reference.status,"concurrent status");for(int i=0;i<16;++i) require(r.m[i]==reference.m[i] || (std::isnan(r.m[i]) && std::isnan(reference.m[i])),"concurrent results");}
    std::cout<<"PASS asymmetry accuracy, phase, quality, ABI guards, concurrent calls\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;} }
