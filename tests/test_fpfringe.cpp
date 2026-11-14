extern "C" {
#include "FPFringe.h"
#include "FPAnalyzer.h"
}
#include "Synthetic.h"
#include "AsymmetrySynthetic.h"
#include <array>
#include <climits>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

void check(bool b,const std::string& s) {if(!b) throw std::runtime_error(s);}
bool equal(double a,double b) {return a==b || (std::isnan(a)&&std::isnan(b));}
synthetic::Parameters parameters(int w,int h) {
    synthetic::Parameters p;p.width=w;p.height=h;p.cx=(w-1)/2.+.3;p.cy=(h-1)/2.-.2;
    const double side=std::min(w,h);p.radius=side*.46;p.spacing=side*side*.035;
    p.amplitude=30000;p.background=1000;p.airy=5;
    return p;
}
template<std::size_t N> struct Guard {double before=123,v[N]{},after=456;void verify(){check(before==123&&after==456,"DBL buffer canary");}};
struct IntGuard {int before=123,v[4]{},after=456;void verify(){check(before==123&&after==456,"I32 canary");}};
void screening() {
    int fp=0,fn=0;
    auto test=[&](const std::string& name,const std::vector<unsigned short>& image,int w,int h,bool expected) {
        int b=-1;check(FPF_CheckFringesU16(image.data(),w,h,&b)==0,"screen completed "+name);
        fp+=b&&!expected;fn+=!b&&expected;
        std::cout<<"screen,"<<name<<",expected="<<expected<<",actual="<<b<<'\n';
        check(b==int(expected),"screen "+name);
    };
    auto p=parameters(320,320);test("resolved",synthetic::frame(p),320,320,true);
    p.amplitude=16;p.noise=.5;test("weak",synthetic::frame(p),320,320,true);
    p.amplitude=1;p.noise=0;test("low-contrast",synthetic::frame(p),320,320,false);
    p=parameters(320,320);p.spacing=4800;p.radius=108;
    auto two=synthetic::frame(p);test("two-strong-rings",two,320,320,true);
    double m[4];check(FPF_AnalyzeU16(two.data(),320,320,1,m)==-4,"two-ring screen independent of strict B");
    p.amplitude=0;p.noise=1000;test("noise",synthetic::frame(p),320,320,false);
    std::vector<unsigned short> blank(320*320,1000);test("blank",blank,320,320,false);
    blank[120*320+130]=65535;test("hot-pixel",blank,320,320,false);
    std::vector<unsigned short> stripes(320*320);
    for(int y=0;y<320;++y) for(int x=0;x<320;++x) stripes[y*320+x]=static_cast<unsigned short>(1000+10000*(1+std::sin(x*.2)));
    test("stripes",stripes,320,320,false);
    std::fill(blank.begin(),blank.end(),65535);test("fully-saturated",blank,320,320,false);
    p=parameters(320,320);auto partial=synthetic::frame(p);
    for(int y=0;y<320;++y) for(int x=0;x<320;++x) if(x>160) partial[y*320+x]=1000;
    test("half-obscured",partial,320,320,false);
    std::cout<<"screen,FP="<<fp<<",FN="<<fn<<",fixtures=10\n";
}
int main() {try {
    check(FPF_GetApiVersion()==1 && FP_GetApiVersion()==2,"versions coexist");
    screening();
    for(auto shape:std::vector<std::array<int,2>>{{320,320},{640,480},{1280,960},{960,1280}}) {
        auto p=parameters(shape[0],shape[1]);auto image=synthetic::frame(p),original=image;
        for(int method=0;method<4;++method) {
            Guard<4> m;int status=FPF_AnalyzeU16(image.data(),p.width,p.height,method,m.v);m.verify();
            std::cout<<"analysis,"<<p.width<<'x'<<p.height<<",method="<<method<<",status="<<status<<",cx="<<m.v[1]<<",cy="<<m.v[2]<<'\n';
            check(status==0,"rectangular method "+std::to_string(method));
            check(std::hypot(m.v[1]-p.cx,m.v[2]-p.cy)<.7,"image-coordinate center");
            check(synthetic::phaseError(m.v[0],p.phase)<.035 && m.v[3]>1,"phase/finesse");
            if(p.width==p.height) {
                const int roi[]={0,0,p.width};int actual[3];double old[4];std::vector<unsigned short> crop(image.size());
                check(FP_AnalyzeFrameU16WithMethod(image.data(),p.width,p.height,roi,method,crop.data(),p.width*p.height,actual,old)==status,"legacy status");
                for(int k=0;k<4;++k) check(equal(old[k],m.v[k]),"exact square numerical equivalence");
            }
        }
        Guard<16> a;Guard<64> s;check(FPF_AnalyzeAsymmetryU16(image.data(),p.width,p.height,a.v,s.v)==0,"rectangular asymmetry");a.verify();s.verify();
        check(std::hypot(a.v[1]-p.cx,a.v[2]-p.cy)<.7 && a.v[13]>=2 && a.v[14]>=10,"asymmetry geometry");
        for(int requested:{0,1,static_cast<int>(std::min(p.width,p.height)*.8)}) {
            IntGuard roi;Guard<2> center;
            check(FPF_FindCenteredRoiU16(image.data(),p.width,p.height,requested,roi.v,center.v)==0,"ROI locator");roi.verify();center.verify();
            check(roi.v[2]==roi.v[3] && (!requested || roi.v[2]==requested),"square exact requested ROI");
            if(!requested) check(roi.v[2]>2*(p.radius-12),"retain weak outer ring support beyond contrast box");
            check(roi.v[0]>=0 && roi.v[1]>=0 && roi.v[0]+roi.v[2]<=p.width && roi.v[1]+roi.v[3]<=p.height,"ROI bounds");
            check(std::abs(center.v[0]-p.cx)<.7 && std::abs(center.v[1]-p.cy)<.7,"independent full-frame center");
            check(std::abs(center.v[0]-roi.v[0]-(roi.v[2]-1)/2.)<=.75 && std::abs(center.v[1]-roi.v[1]-(roi.v[3]-1)/2.)<=.75,"ROI centering");
        }
        check(image==original,"all APIs preserve source");
    }
    // Known angular gradients on the full landscape/portrait image, including
    // native row strides and coordinates beyond any implicit square origin.
    for(auto shape:std::vector<std::array<int,2>>{{400,320},{320,400}}) {
        const int width=shape[0],height=shape[1];
        std::vector<unsigned short> pixels(static_cast<std::size_t>(width)*height);
        const double cx=(width-1)/2.+.3,cy=(height-1)/2.-.2;
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
            const double dx=x-cx,dy=y-cy,t=std::atan2(dy,dx);
            const double w=.24*(1-.18*std::cos(t)+.12*std::sin(t));
            const double f=1/std::pow(std::sin(synthetic::pi*w/2),2)-2,b=1/(1+f);
            const double sine=std::sin(synthetic::pi*((dx*dx+dy*dy)/3600+.27));
            pixels[static_cast<std::size_t>(y)*width+x]=static_cast<unsigned short>(std::lround(65535*(.05+.60*(1+.25*std::cos(t)-.15*std::sin(t))*(1/(1+f*sine*sine)-b)/(1-b))));
        }
        double a[16],s[64];check(FPF_AnalyzeAsymmetryU16(pixels.data(),width,height,a,s)==0,"rectangular angular-gradient success");
        check(std::abs(a[4]-.25)<.04 && std::abs(a[5]+.15)<.04 && std::abs(a[6]+.18)<.04 && std::abs(a[7]-.12)<.04,"rectangular signed coefficients");
        check(std::hypot(a[1]-cx,a[2]-cy)<.7,"rectangular angular center");
    }
    // An off-center rectangular pattern must retain coordinates outside the
    // implicit centered-square origin; no hidden square selection is permitted.
    auto off=parameters(640,480);off.cx=480.3;off.cy=220.7;off.radius=145;off.spacing=2500;
    auto offImage=synthetic::frame(off);double m[4];
    check(FPF_AnalyzeU16(offImage.data(),640,480,1,m)==0 && std::abs(m[1]-off.cx)<.7,"off-center rectangular B");
    int roi[4];double center[2];
    check(FPF_FindCenteredRoiU16(offImage.data(),640,480,0,roi,center)==0,"off-center auto ROI");
    check(FPF_FindCenteredRoiU16(offImage.data(),640,480,400,roi,center)==-1,"impossible exact centered size");
    for(int v:roi) check(v==0,"failed ROI is zero");for(double v:center) check(std::isnan(v),"failed center NaN");
    for(int bits:{8,10,12,14,16}) {
        asymSynthetic::Parameters p;p.bits=bits;p.ax=.25;p.ay=-.15;p.wx=-.18;p.wy=.12;
        auto image=asymSynthetic::frame(p);const int r[]={0,0,320};int actual[3];double old[16],os[64],a[16],s[64];
        check(FP_AnalyzeFringeAsymmetryU16(image.data(),320,320,r,actual,old,os)==0,"legacy asymmetry fixture");
        check(FPF_AnalyzeAsymmetryU16(image.data(),320,320,a,s)==0,"new asymmetry fixture");
        for(int k=0;k<16;++k) check(equal(old[k],a[k]),"exact asymmetry summary equivalence");
        for(int k=0;k<64;++k) check(equal(os[k],s[k]),"exact sector equivalence");
        check(std::abs(a[4]-p.ax)<.04 && std::abs(a[5]-p.ay)<.04 && std::abs(a[6]-p.wx)<.04 && std::abs(a[7]-p.wy)<.04,"known signed asymmetry");
    }
    auto p=parameters(320,320);auto image=synthetic::frame(p);
    auto call=[&] {
        std::array<double,4> a;int localRoi[4],b;double localCenter[2],summary[16],sectors[64];
        check(FPF_AnalyzeU16(image.data(),320,320,1,a.data())==0,"thread result");
        check(FPF_CheckFringesU16(image.data(),320,320,&b)==0 && b==1,"thread screening");
        check(FPF_FindCenteredRoiU16(image.data(),320,320,240,localRoi,localCenter)==0,"thread ROI");
        check(FPF_AnalyzeAsymmetryU16(image.data(),320,320,summary,sectors)==0,"thread asymmetry");
        return a;
    };
    auto expected=call();std::vector<std::future<std::array<double,4>>> threads;
    for(int i=0;i<8;++i) threads.push_back(std::async(std::launch::async,call));
    for(auto& t:threads) check(t.get()==expected,"reentrant exact results");
    for(int method:{-1,4,INT_MAX}) {check(FPF_AnalyzeU16(image.data(),320,320,method,m)==-5,"invalid method");for(double v:m) check(std::isnan(v),"invalid method NaN");}
    check(FPF_AnalyzeU16(nullptr,320,320,1,m)==-1,"null image");
    check(FPF_AnalyzeU16(image.data(),INT_MAX,INT_MAX,1,m)==-1,"dimension overflow");
    check(FPF_AnalyzeU16(image.data(),320,320,1,nullptr)==-1,"null metrics");
    check(FPF_CheckFringesU16(image.data(),320,320,nullptr)==-1,"null boolean");
    check(FPF_FindCenteredRoiU16(image.data(),320,320,-1,roi,center)==-1,"negative size");
    check(FPF_FindCenteredRoiU16(image.data(),320,320,0,nullptr,center)==-1,"null ROI");
    check(FPF_FindCenteredRoiU16(image.data(),320,320,0,roi,nullptr)==-1,"null center");
    double a[16],s[64];check(FPF_AnalyzeAsymmetryU16(image.data(),320,320,a,a)==-1,"overlap outputs");
    check(FPF_AnalyzeAsymmetryU16(image.data(),320,320,nullptr,s)==-1,"null summary");
    check(FPF_AnalyzeAsymmetryU16(image.data(),320,320,a,nullptr)==-1,"null sectors");
    auto original=image;check(FPF_CheckFringesU16(image.data(),320,320,reinterpret_cast<int*>(image.data()))==-1 && image==original,"input overlap untouched");
    unsigned short tiny=0;check(FPF_AnalyzeU16(&tiny,1,1,1,m)==-4,"tiny valid image");
    int b=123;check(FPF_CheckFringesU16(&tiny,1,1,&b)==0 && b==0,"tiny screen negative");
    std::cout<<"PASS FPFringe: rectangular methods, ROI, screening, exact legacy equivalence, ABI and concurrency\n";return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;} }
