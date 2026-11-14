#include "FringeDetection.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
double sample(fp::PixelView p,int width,double x,double y) {
    const int ix=static_cast<int>(x),iy=static_cast<int>(y);
    const double fx=x-ix,fy=y-iy;
    const auto i=static_cast<std::size_t>(iy)*width+ix;
    return (1-fy)*((1-fx)*p[i]+fx*p[i+1])+fy*((1-fx)*p[i+width]+fx*p[i+width+1]);
}
// Internal ring-evidence validation used by the ROI finder.
// The fitted center is supplied by the locator; verify radial/angular support.
bool evidence(fp::PixelView view,int width,int height,fp::Center c) {
    const auto profile=fp::radialProfile(view,width,height,c);
    const auto& p=profile.intensity;
    const int n=static_cast<int>(p.size());
    if(n<40) return false;
    const auto extrema=std::minmax_element(p.begin(),p.end());
    const double span=*extrema.second-*extrema.first;
    if(span<10 || span/view.gain<2) return false;
    const int window=std::max(6,n/16);
    std::vector<int> peaks;
    for(int i=3;i<n-3;++i) if(p[i]>p[i-1] && p[i]>=p[i+1]) {
        const int lo=std::max(0,i-window),hi=std::min(n-1,i+window);
        const double baseline=std::max(*std::min_element(p.begin()+lo,p.begin()+i),
                                      *std::min_element(p.begin()+i+1,p.begin()+hi+1));
        if(p[i]-baseline<.20*span || (!peaks.empty() && i-peaks.back()<12)) continue;
        peaks.push_back(i);
    }
    if(peaks.size()<2) return false;
    int supported=0;
    for(std::size_t k=0;k<peaks.size();++k) {
        const double q=peaks[k]*profile.step;
        const double gap=(k ? peaks[k]-peaks[k-1] : peaks[1]-peaks[0])*profile.step;
        const double ql=q-.25*gap,qr=q+.25*gap;
        const double radius=std::min({c.x,c.y,width-1-c.x,height-1-c.y})-2;
        if(ql<4 || qr>radius*radius) continue;
        int good=0,run=0,maxGap=0;
        bool valid[32]={};
        for(int j=0;j<32;++j) {
            const double t=2*pi*j/32,ct=std::cos(t),st=std::sin(t);
            const auto at=[&](double qq){const double r=std::sqrt(qq);return sample(view,width,c.x+r*ct,c.y+r*st);};
            valid[j]=at(q)-(at(ql)+at(qr))/2>.10*span;
            good+=valid[j];
        }
        for(int j=0;j<64;++j) {run=valid[j%32] ? 0 : run+1;maxGap=std::max(maxGap,run);}
        if(good>=24 && maxGap<=8 && ++supported>=2) return true;
    }
    return false;
}
}

int fp::findCenteredRoi(PixelView view,int width,int height,int requested,int* roiOut,double* centerOut) {
    Roi candidate;
    if(!detectRoi(view,width,height,candidate)) return -3;
    std::vector<unsigned short> temp(static_cast<std::size_t>(candidate.size)*candidate.size);
    for(int y=0;y<candidate.size;++y)
        std::copy_n(view.pixels+static_cast<std::size_t>(y+candidate.top)*width+candidate.left,candidate.size,temp.data()+static_cast<std::size_t>(y)*candidate.size);
    Center c;
    if(!estimateCenter({temp.data(),view.offset,view.gain},candidate.size,c)) return -4;
    c.x+=candidate.left;c.y+=candidate.top;
    if(!evidence(view,width,height,c)) return -3;
    // Retain every complete circle that can fit about the independently fitted
    // center, rather than truncating weak outer rings to the contrast box.
    const double margin=std::min({c.x,c.y,width-1-c.x,height-1-c.y});
    const int automatic=static_cast<int>(std::floor(2*margin+1));
    const int size=requested ? requested : automatic;
    if((!requested && size<24) || size<=0 || size>width || size>height) return requested ? -1 : -4;
    const int left=static_cast<int>(std::lround(c.x-(size-1)/2.0));
    const int top=static_cast<int>(std::lround(c.y-(size-1)/2.0));
    if(left<0 || top<0 || left>width-size || top>height-size ||
       std::abs(c.x-left-(size-1)/2.0)>.75 || std::abs(c.y-top-(size-1)/2.0)>.75)
        return requested ? -1 : -4;
    roiOut[0]=left;roiOut[1]=top;roiOut[2]=roiOut[3]=size;
    centerOut[0]=c.x;centerOut[1]=c.y;
    return 0;
}
