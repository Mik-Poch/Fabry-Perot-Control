extern "C" {
#include "FPFringe.h"
#include "FPAnalyzer.h"
}
#include "Synthetic.h"
#include <array>
#include <climits>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
constexpr unsigned short sentinel=0xA55A;
void require(bool b,const std::string& what) {if(!b) throw std::runtime_error(what);}
bool same(double a,double b) {return a==b || (std::isnan(a)&&std::isnan(b));}
struct Result {int status;std::array<int,3> roi;std::vector<unsigned short> crop;};
Result locate(const std::vector<unsigned short>& image,int width,int height,int requested) {
    const auto original=image;
    struct {int before=123,values[3]={7,8,9},after=456;} roi;
    const int capacity=std::min(width,height)*std::min(width,height);
    std::vector<unsigned short> output(static_cast<std::size_t>(capacity)+2,sentinel);
    const int status=FPF_FindCenteredRoiU16(image.data(),width,height,requested,output.data()+1,capacity,roi.values);
    require(image==original,"source must remain unchanged");
    require(roi.before==123 && roi.after==456,"ROI exactly three I32 canaries");
    require(output.front()==sentinel && output.back()==sentinel,"U16 canaries");
    const int count=status==0 ? roi.values[2]*roi.values[2] : 0;
    for(std::size_t j=static_cast<std::size_t>(count)+1;j<output.size();++j) require(output[j]==sentinel,"unused crop tail unchanged");
    if(status==0) {
        const int left=roi.values[0],top=roi.values[1],side=roi.values[2];
        require(side>0 && left>=0 && top>=0 && left<=width-side && top<=height-side,"square in-bounds geometry");
        for(int y=0;y<side;++y) for(int x=0;x<side;++x)
            require(output[1+static_cast<std::size_t>(y)*side+x]==image[static_cast<std::size_t>(top+y)*width+left+x],"bit-exact source U16 crop");
    } else for(int v:roi.values) require(v==0,"failure ROI zero");
    return {status,{roi.values[0],roi.values[1],roi.values[2]},std::vector<unsigned short>(output.begin()+1,output.begin()+1+count)};
}
synthetic::Parameters fixture(int w,int h,double cx,double cy,double radius) {
    synthetic::Parameters p;p.width=w;p.height=h;p.cx=cx;p.cy=cy;p.radius=radius;
    p.spacing=2800;p.airy=5;p.amplitude=48000;p.background=400;p.phase=.27;return p;
}
void compareAnalyses(const Result& r) {
    const int side=r.roi[2],manual[]={0,0,side};
    std::vector<unsigned short> legacyCrop(r.crop.size());int oldRoi[3];
    for(int method=0;method<4;++method) {
        double old[4],current[4];
        const int a=FP_AnalyzeFrameU16WithMethod(r.crop.data(),side,side,manual,method,legacyCrop.data(),side*side,oldRoi,old);
        const int b=FPF_AnalyzeU16(r.crop.data(),side,side,method,current);
        require(a==b,"same-pixel analysis statuses");
        require(b==0,"end-to-end selected analysis succeeds");
        for(int k=0;k<4;++k) require(same(old[k],current[k]),"same-pixel analysis output unchanged");
    }
    double old[16],os[64],current[16],cs[64];
    const int a=FP_AnalyzeFringeAsymmetryU16(r.crop.data(),side,side,manual,oldRoi,old,os);
    const int b=FPF_AnalyzeAsymmetryU16(r.crop.data(),side,side,current,cs);
    require(a==b && b==0,"end-to-end asymmetry succeeds");
    for(int k=0;k<16;++k) require(same(old[k],current[k]),"same-pixel summary unchanged");
    for(int k=0;k<64;++k) require(same(os[k],cs[k]),"same-pixel sectors unchanged");
}
int main() {try {
    std::vector<synthetic::Parameters> cases={
        fixture(512,512,255.3,255.7,140),fixture(640,480,480.3,220.7,125),
        fixture(480,640,220.3,420.7,125),fixture(1280,960,659.0,469.4,132)};
    int index=0;
    for(auto p:cases) {
        auto image=synthetic::frame(p);
        auto r=locate(image,p.width,p.height,0);
        require(r.status==0,"automatic fixture "+std::to_string(index));
        require(std::abs(p.cx-r.roi[0]-(r.roi[2]-1)/2.)<=.75 && std::abs(p.cy-r.roi[1]-(r.roi[2]-1)/2.)<=.75,"subpixel truth within geometric midpoint tolerance");
        require(r.roi[2]>=2*(p.radius-20),"visible ring neighborhood retained");
        if(p.width==1280) {require(r.roi[2]<512,"compact pattern must not produce near-full-frame crop");compareAnalyses(r);}
        for(int side:{1,240}) {auto exact=locate(image,p.width,p.height,side);require(exact.status==0 && exact.roi[2]==side,"exact requested size");}
        std::cout<<"ROI,"<<index++<<",frame="<<p.width<<'x'<<p.height<<",left="<<r.roi[0]<<",top="<<r.roi[1]<<",side="<<r.roi[2]<<'\n';
    }
    // Strong circular central brightness with a weak outer fringe envelope.
    auto weak=fixture(1280,960,659.0,469.4,150);auto image=synthetic::frame(weak);
    for(int y=0;y<weak.height;++y) for(int x=0;x<weak.width;++x) {
        const double q=(x-weak.cx)*(x-weak.cx)+(y-weak.cy)*(y-weak.cy);
        const auto i=static_cast<std::size_t>(y)*weak.width+x;
        const double value=weak.background+(image[i]-weak.background)*std::exp(-q/(2*90.*90.))+12000*std::exp(-q/(2*10.*10.));
        image[i]=static_cast<unsigned short>(std::lround(value));
    }
    auto weakRoi=locate(image,weak.width,weak.height,0);require(weakRoi.status==0,"bright center with weak outer rings");
    require(weakRoi.roi[2]<512,"weak compact crop size");
    require(std::abs(weak.cx-weakRoi.roi[0]-(weakRoi.roi[2]-1)/2.)<=.75 && std::abs(weak.cy-weakRoi.roi[1]-(weakRoi.roi[2]-1)/2.)<=.75,"weak-pattern centering");
    // A single circular fringe is enough to fit a center. It must not be
    // rejected by the former post-fit multi-ring evidence/analysis gate.
    auto one=fixture(1280,960,659.0,469.4,70);one.spacing=10000;one.phase=.7;
    require(locate(synthetic::frame(one),one.width,one.height,0).status==0,"no strict ring-count post-fit gate");
    // Capacity discovery/retry never writes pixels on -2, including a short
    // nonzero buffer. Exactly side*side values are valid on success.
    auto p=cases.back();image=synthetic::frame(p);unsigned short placeholder=sentinel;int roi[3]={7,8,9};
    require(FPF_FindCenteredRoiU16(image.data(),p.width,p.height,0,&placeholder,0,roi)==-2 && placeholder==sentinel,"zero-capacity geometry query");
    const std::array<int,3> geometry={roi[0],roi[1],roi[2]};const int count=roi[2]*roi[2];
    std::vector<unsigned short> shortCrop(static_cast<std::size_t>(count)+2,sentinel);
    require(FPF_FindCenteredRoiU16(image.data(),p.width,p.height,0,shortCrop.data()+1,count-1,roi)==-2,"short capacity");
    require(std::all_of(shortCrop.begin(),shortCrop.end(),[](auto v){return v==sentinel;}),"no partial crop on capacity failure");
    require(std::equal(roi,roi+3,geometry.begin()),"capacity geometry deterministic");
    require(FPF_FindCenteredRoiU16(image.data(),p.width,p.height,0,shortCrop.data()+1,count,roi)==0,"exact capacity retry");
    require(shortCrop.front()==sentinel && shortCrop.back()==sentinel,"exact allocation guards");
    auto expectInvalid=[&](const unsigned short* input,int w,int h,int request,unsigned short* output,int capacity,int* ro) {
        require(FPF_FindCenteredRoiU16(input,w,h,request,output,capacity,ro)==-1,"invalid arguments");
        if(ro) require(ro[0]==0 && ro[1]==0 && ro[2]==0,"initialized invalid ROI");
    };
    expectInvalid(nullptr,p.width,p.height,0,&placeholder,0,roi);
    expectInvalid(image.data(),0,p.height,0,&placeholder,0,roi);
    expectInvalid(image.data(),INT_MAX,INT_MAX,0,&placeholder,0,roi);
    expectInvalid(image.data(),p.width,p.height,-1,&placeholder,0,roi);
    expectInvalid(image.data(),p.width,p.height,INT_MAX,&placeholder,0,roi);
    expectInvalid(image.data(),p.width,p.height,0,&placeholder,-1,roi);
    expectInvalid(image.data(),p.width,p.height,0,nullptr,0,roi);
    expectInvalid(image.data(),p.width,p.height,0,&placeholder,0,nullptr);
    require(placeholder==sentinel,"invalid calls do not modify pixels");
    auto off=fixture(640,480,480.3,220.7,125);auto offPixels=synthetic::frame(off);
    expectInvalid(offPixels.data(),640,480,400,&placeholder,0,roi);
    std::vector<unsigned short> blank(640*480,0);
    require(FPF_FindCenteredRoiU16(blank.data(),640,480,0,&placeholder,0,roi)==-3,"no candidate");
    for(int y=0;y<480;++y) for(int x=0;x<640;++x) blank[y*640+x]=static_cast<unsigned short>(1000+10000*(1+std::sin(x*.2)));
    require(FPF_FindCenteredRoiU16(blank.data(),640,480,0,&placeholder,0,roi)==-4,"unreliable center");
    require(roi[0]==0 && roi[1]==0 && roi[2]==0 && placeholder==sentinel,"failure outputs unchanged");
    auto original=image;roi[0]=7;roi[1]=8;roi[2]=9;
    require(FPF_FindCenteredRoiU16(image.data(),p.width,p.height,0,image.data(),static_cast<int>(image.size()),roi)==-1 && image==original && roi[0]==7,"input/output overlap before any writes");
    std::vector<unsigned short> overlap(32,sentinel);
    require(FPF_FindCenteredRoiU16(image.data(),p.width,p.height,0,overlap.data(),32,reinterpret_cast<int*>(overlap.data()+2))==-1,"partial crop/geometry overlap");
    require(std::all_of(overlap.begin(),overlap.end(),[](auto v){return v==sentinel;}),"overlap writes nothing");
    auto reference=locate(image,p.width,p.height,0);std::vector<std::future<Result>> jobs;
    for(int k=0;k<8;++k) jobs.push_back(std::async(std::launch::async,[&]{return locate(image,p.width,p.height,0);}));
    for(auto& job:jobs) {auto r=job.get();require(r.status==reference.status && r.roi==reference.roi && r.crop==reference.crop,"concurrent identical extraction");}
    std::cout<<"PASS API3 crop: local compact/weak/single rings, geometry, original U16, capacity retry, invalid inputs, failures, concurrency and end-to-end legacy equivalence\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
