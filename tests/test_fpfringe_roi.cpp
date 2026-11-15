extern "C" {
#include "FPFringe.h"
#include "FPAnalyzer.h"
}
#include "Synthetic.h"
#include <array>
#include <climits>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
constexpr unsigned short sentinel=0xA55A;
void require(bool b,const std::string& what) {if(!b) throw std::runtime_error(what);}
struct RoiGuard {int before=123,values[3]={7,8,9},after=456;};
struct Result {int status;std::array<int,3> roi;std::vector<unsigned short> pixels;};
Result compare(const std::vector<unsigned short>& source,int width,int height,int capacity,const std::string& name,
               bool nullImage=false,bool nullCrop=false,bool nullRoi=false) {
    auto image=source;const auto original=image;
    const auto allocated=static_cast<std::size_t>(std::max(0,capacity));
    std::vector<unsigned short> old(allocated+2,sentinel),current(allocated+2,sentinel);
    RoiGuard a,b;const int automatic[3]={0,0,0};double metrics[4];
    const auto* input=nullImage?nullptr:image.data();
    const int previous=FP_AnalyzeFrameU16WithMethod(input,width,height,automatic,0,nullCrop?nullptr:old.data()+1,capacity,nullRoi?nullptr:a.values,metrics);
    const int status=FPF_FindCenteredRoiU16(input,width,height,nullCrop?nullptr:current.data()+1,capacity,nullRoi?nullptr:b.values);
    require(status==previous,name+": identical status");
    require(std::equal(a.values,a.values+3,b.values),name+": identical ROI");
    require(std::memcmp(old.data(),current.data(),old.size()*sizeof(unsigned short))==0,name+": byte-identical complete output buffers");
    require(image==original,name+": source unchanged");
    require(a.before==123 && a.after==456 && b.before==123 && b.after==456,name+": ROI canaries");
    require(old.front()==sentinel && old.back()==sentinel && current.front()==sentinel && current.back()==sentinel,name+": crop canaries");
    if(status==-2) require(std::all_of(current.begin(),current.end(),[](auto v){return v==sentinel;}),name+": no crop writes on -2");
    const int side=b.values[2];
    if(!nullRoi && !nullCrop && (status==0 || status==-4) && side>0) {
        require(side<=width && side<=height && b.values[0]>=0 && b.values[1]>=0 && b.values[0]<=width-side && b.values[1]<=height-side,name+": bounds");
        require(static_cast<std::size_t>(side)*side<=allocated,name+": valid copied size");
        for(int y=0;y<side;++y) for(int x=0;x<side;++x)
            require(current[1+static_cast<std::size_t>(y)*side+x]==source[static_cast<std::size_t>(y+b.values[1])*width+b.values[0]+x],name+": original U16 pixels");
        for(std::size_t i=static_cast<std::size_t>(side)*side+1;i<current.size();++i) require(current[i]==sentinel,name+": untouched tail");
    }
    if(name!="thread") std::cout<<name<<",status="<<status<<",legacy="<<previous<<",roi="<<b.values[0]<<':'<<b.values[1]<<':'<<side<<",capacity="<<capacity<<'\n';
    return {status,{b.values[0],b.values[1],b.values[2]},std::move(current)};
}
synthetic::Parameters fixture(int w,int h,double cx,double cy,double radius) {
    synthetic::Parameters p;p.width=w;p.height=h;p.cx=cx;p.cy=cy;p.radius=radius;
    p.spacing=2800;p.airy=5;p.amplitude=48000;p.background=400;p.phase=.27;return p;
}
int main() {try {
    require(FPF_GetApiVersion()==4 && FP_GetApiVersion()==2,"independent API versions");
    std::vector<synthetic::Parameters> cases={
        fixture(320,320,159.3,159.7,140),fixture(640,480,319.3,239.7,180),
        fixture(480,640,239.3,319.7,180),fixture(1280,960,659.0,469.4,132),
        fixture(640,480,480.3,220.7,125)};
    int index=0,successes=0;
    for(const auto& p:cases) {
        auto image=synthetic::frame(p);const int capacity=std::min(p.width,p.height)*std::min(p.width,p.height);
        auto result=compare(image,p.width,p.height,capacity,"fixture-"+std::to_string(index++));successes+=result.status==0;
        auto query=compare(image,p.width,p.height,0,"zero-capacity");
        require(query.status==-2 && query.roi[2]>0,"legacy capacity query");
        const int required=query.roi[2]*query.roi[2];
        require(compare(image,p.width,p.height,required-1,"short-capacity").status==-2,"legacy short capacity");
        compare(image,p.width,p.height,required,"exact-capacity");
    }
    require(successes>0,"positive method-0 calls executed");
    auto p=cases[3];auto image=synthetic::frame(p);
    require(std::min(p.width,p.height)*std::min(p.width,p.height)==921600,"1280x960 actual allocation regression");
    auto weak=image;
    for(int y=0;y<p.height;++y) for(int x=0;x<p.width;++x) {
        const double q=(x-p.cx)*(x-p.cx)+(y-p.cy)*(y-p.cy);const auto i=static_cast<std::size_t>(y)*p.width+x;
        weak[i]=static_cast<unsigned short>(std::lround(p.background+(weak[i]-p.background)*std::exp(-q/(2*90.*90.))+12000*std::exp(-q/(2*10.*10.))));
    }
    compare(weak,p.width,p.height,921600,"bright-center-weak-outer");
    auto one=p;one.radius=70;one.spacing=10000;one.phase=.7;
    compare(synthetic::frame(one),one.width,one.height,921600,"single-ring-preserve-status");
    std::vector<unsigned short> blank(640*480,0);
    require(compare(blank,640,480,480*480,"blank").status==-3,"legacy no candidate");
    for(int y=0;y<480;++y) for(int x=0;x<640;++x) blank[y*640+x]=static_cast<unsigned short>(1000+10000*(1+std::sin(x*.2)));
    auto failed=compare(blank,640,480,480*480,"stripes-analysis-failure");
    require(failed.status==-4 && failed.roi[2]>0,"legacy -4 retains a copied crop");
    compare(image,p.width,p.height,1,"null-image",true);
    compare(image,p.width,p.height,1,"null-crop",false,true);
    compare(image,p.width,p.height,1,"null-roi",false,false,true);
    compare(image,0,p.height,1,"zero-width");compare(image,-1,p.height,1,"negative-width");
    compare(image,INT_MAX,INT_MAX,1,"dimension-overflow");compare(image,p.width,p.height,-1,"negative-capacity");
    // Known overlaps are passed identically to both APIs and rejected before
    // initialization. No invalid allocation is deliberately dereferenced.
    auto a=image,b=image;RoiGuard ar,br;const int automatic[3]={0,0,0};double m[4];
    const int oldStatus=FP_AnalyzeFrameU16WithMethod(a.data(),p.width,p.height,automatic,0,a.data(),static_cast<int>(a.size()),ar.values,m);
    const int status=FPF_FindCenteredRoiU16(b.data(),p.width,p.height,b.data(),static_cast<int>(b.size()),br.values);
    require(status==oldStatus && status==-1 && a==image && b==image && std::equal(ar.values,ar.values+3,br.values),"source/crop overlap exact legacy behavior");
    std::vector<unsigned short> oldOverlap(32,sentinel),newOverlap(32,sentinel);
    const int oa=FP_AnalyzeFrameU16WithMethod(image.data(),p.width,p.height,automatic,0,oldOverlap.data(),32,reinterpret_cast<int*>(oldOverlap.data()+2),m);
    const int ob=FPF_FindCenteredRoiU16(image.data(),p.width,p.height,newOverlap.data(),32,reinterpret_cast<int*>(newOverlap.data()+2));
    require(oa==ob && ob==-1 && oldOverlap==newOverlap,"partial output overlap exact behavior");
    auto reference=compare(image,p.width,p.height,921600,"thread-reference");
    std::vector<std::future<Result>> jobs;
    for(int k=0;k<8;++k) jobs.push_back(std::async(std::launch::async,[&]{return compare(image,p.width,p.height,921600,"thread");}));
    for(auto& job:jobs) {auto r=job.get();require(r.status==reference.status && r.roi==reference.roi && r.pixels==reference.pixels,"concurrent identical results");}
    std::cout<<"PASS API4: exact legacy automatic method-0 statuses, ROI and crop bytes; square/rectangular, actual 921600 U16 allocation, capacities, failures, canaries, overlap and concurrent calls\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
