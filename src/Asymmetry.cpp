#include "Asymmetry.h"
#include "Analysis.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr int sectors=16, rays=9;
constexpr double missing=std::numeric_limits<double>::quiet_NaN();
struct Peak { double prominence=0, width=0; };
double median(std::vector<double> v) {
    if(v.empty()) return missing;
    const auto mid=v.begin()+v.size()/2;
    std::nth_element(v.begin(),mid,v.end());
    const double hi=*mid;
    return v.size()%2 ? hi : (hi+*std::max_element(v.begin(),mid))/2;
}
bool overlaps(const void* a,std::size_t na,const void* b,std::size_t nb) {
    if(!a || !b || !na || !nb) return false;
    const auto x=reinterpret_cast<std::uintptr_t>(a), y=reinterpret_cast<std::uintptr_t>(b);
    return x<=y ? y-x<na : x-y<nb;
}
double sample(fp::PixelView p,int size,double x,double y) {
    const int ix=static_cast<int>(x), iy=static_cast<int>(y);
    const double fx=x-ix,fy=y-iy;
    const auto i=static_cast<std::size_t>(iy)*size+ix;
    return (1-fy)*((1-fx)*p[i]+fx*p[i+1])+fy*((1-fx)*p[i+size]+fx*p[i+size+1]);
}
// Measure each ray BEFORE angular aggregation: center errors cannot smear a
// sector-averaged peak. The disjoint +/-0.20P peak gates preserve physical identity.
Peak measureRay(fp::PixelView p,int size,fp::Center c,double theta,double q,double spacing,std::vector<double>& v) {
    const double lo=std::sqrt(q-.52*spacing), hi=std::sqrt(q+.52*spacing);
    const int n=static_cast<int>(std::ceil((hi-lo)/.25))+1;
    if(n<12) return {};
    const double dr=(hi-lo)/(n-1), ct=std::cos(theta), st=std::sin(theta);
    v.resize(n);
    for(int i=0;i<n;++i) {
        const double r=lo+i*dr;
        v[i]=sample(p,size,c.x+r*ct,c.y+r*st);
    }
    const auto gate=[&](double dq) {return std::clamp(static_cast<int>((std::sqrt(q+dq*spacing)-lo)/dr),1,n-2);};
    const int a=gate(-.20),b=gate(.20);
    const int k=static_cast<int>(std::max_element(v.begin()+a,v.begin()+b+1)-v.begin());
    if(k<=a || k>=b) return {};
    const double curvature=v[k-1]-2*v[k]+v[k+1];
    if(curvature>=-1e-9) return {};
    const double delta=std::clamp(.5*(v[k-1]-v[k+1])/curvature,-.5,.5);
    const double peak=v[k]-.25*(v[k-1]-v[k+1])*delta;
    const auto lmin=std::min_element(v.begin(),v.begin()+k);
    const auto rmin=std::min_element(v.begin()+k+1,v.end());
    const double baseline=(*lmin+*rmin)/2, prominence=peak-baseline;
    if(prominence<3 || std::abs(*lmin-*rmin)>.30*prominence) return {};
    const double half=baseline+prominence/2;
    int l=k,r=k;
    while(l>0 && v[l]>half) --l;
    while(r<n-1 && v[r]>half) ++r;
    if(l==0 || r==n-1 || l==k || r==k || v[l+1]<=v[l] || v[r-1]<=v[r]) return {};
    const double left=lo+dr*(l+(half-v[l])/(v[l+1]-v[l]));
    const double right=lo+dr*(r-1+(v[r-1]-half)/(v[r-1]-v[r]));
    const double width=right*right-left*left;
    // Under-resolved widths carry angle-dependent pixel interpolation bias.
    // Reject rather than interpreting them as optical linewidth changes.
    if(right-left<2.0 || width>.55*spacing) return {};
    // Raw saturation is checked independently of the affine analysis view.
    const double rp=lo+dr*(k+delta);
    const int px=static_cast<int>(std::lround(c.x+rp*ct)), py=static_cast<int>(std::lround(c.y+rp*st));
    if(p.pixels[static_cast<std::size_t>(py)*size+px]>=65534) return {};
    return {prominence,width};
}
bool solve(double a[3][4],std::array<double,3>& x) {
    for(int k=0;k<3;++k) {
        int best=k;
        for(int i=k+1;i<3;++i) if(std::abs(a[i][k])>std::abs(a[best][k])) best=i;
        if(std::abs(a[best][k])<1e-7) return false;
        for(int j=k;j<4;++j) std::swap(a[k][j],a[best][j]);
        const double d=a[k][k];
        for(int j=k;j<4;++j) a[k][j]/=d;
        for(int i=0;i<3;++i) if(i!=k) {
            const double f=a[i][k];
            for(int j=k;j<4;++j) a[i][j]-=f*a[k][j];
        }
    }
    for(int i=0;i<3;++i) x[i]=a[i][3];
    return true;
}
// Intercept + signed X/Y, Huber IRLS. Equal initial sector weights avoid
// overweighting bright directions; missing sectors are never replaced by 1.
bool harmonic(const std::array<double,16>& v,std::array<double,3>& fit) {
    std::array<double,16> weights; weights.fill(1);
    for(int iteration=0;iteration<6;++iteration) {
        double a[3][4]={};
        for(int s=0;s<16;++s) if(std::isfinite(v[s])) {
            const double t=2*pi*s/16, row[]={1,std::cos(t),std::sin(t)};
            for(int i=0;i<3;++i) {
                for(int j=0;j<3;++j) a[i][j]+=weights[s]*row[i]*row[j];
                a[i][3]+=weights[s]*row[i]*v[s];
            }
        }
        if(!solve(a,fit)) return false;
        std::vector<double> errors;
        for(int s=0;s<16;++s) if(std::isfinite(v[s])) errors.push_back(std::abs(v[s]-fit[0]-fit[1]*std::cos(2*pi*s/16)-fit[2]*std::sin(2*pi*s/16)));
        const double scale=std::max(.015,1.4826*median(errors));
        for(int s=0;s<16;++s) if(std::isfinite(v[s])) {
            const double e=std::abs(v[s]-fit[0]-fit[1]*std::cos(2*pi*s/16)-fit[2]*std::sin(2*pi*s/16));
            weights[s]=e>1.5*scale ? 1.5*scale/e : 1;
        }
    }
    return true;
}
bool coverage(const std::array<bool,16>& valid) {
    int count=0,gap=0,maxGap=0;
    for(bool v:valid) count+=v;
    for(int i=0;i<32;++i) {gap=valid[i%16] ? 0 : gap+1;maxGap=std::max(maxGap,gap);}
    return count>=10 && maxGap<=4;
}
double direction(double x,double y) {
    double d=std::atan2(y,x)*180/pi;
    return d<0 ? d+360 : d;
}
bool analyze(fp::PixelView p,int size,double* out,double* sector) {
    fp::Center c;
    fp::Measurement m;
    fp::ProfileFit identities;
    if(!fp::estimateCenter(p,size,c) || !fp::measureProfile(fp::radialProfile(p,size,c),m,&identities)) return false;
    const double radius=std::min({c.x,c.y,size-1-c.x,size-1-c.y})-2;
    std::array<std::vector<double>,16> ps,ws;
    std::vector<double> widths;
    int rings=0;
    std::vector<double> rayProfile;
    for(double q:identities.peakQ) {
        if(q-.52*m.spacing<16 || q+.52*m.spacing>radius*radius) continue;
        std::array<Peak,16> peaks{};
        std::array<bool,16> valid{};
        double sumP=0,sumW=0;int n=0;
        for(int s=0;s<16;++s) {
            std::vector<double> prom,width;
            for(int j=0;j<rays;++j) {
                const double theta=2*pi*(s+(j+.5)/rays-.5)/16;
                const Peak v=measureRay(p,size,c,theta,q,m.spacing,rayProfile);
                if(v.width>0) {prom.push_back(v.prominence);width.push_back(v.width);}
            }
            if(prom.size()<6) continue;
            const double pp=median(prom), ww=median(width);
            std::vector<double> deviation;
            for(double x:width) deviation.push_back(std::abs(x/ww-1));
            if(median(deviation)>.15) continue;
            peaks[s]={pp,ww};valid[s]=true;sumP+=pp;sumW+=ww;++n;
        }
        if(!coverage(valid)) continue;
        ++rings;
        widths.push_back(sumW/n);
        for(int s=0;s<16;++s) if(valid[s]) {ps[s].push_back(peaks[s].prominence*n/sumP);ws[s].push_back(peaks[s].width*n/sumW);}
    }
    if(rings<2) return false;
    std::array<double,16> pv,wv;pv.fill(missing);wv.fill(missing);
    std::array<bool,16> valid{};int count=0;
    for(int s=0;s<16;++s) if(ps[s].size()>=2) {
        pv[s]=median(ps[s]);wv[s]=median(ws[s]);
        // Inconsistent normalized profiles across physical rings are unreliable.
        std::vector<double> deviations;
        for(std::size_t j=0;j<ps[s].size();++j) deviations.push_back(std::max(std::abs(ps[s][j]-pv[s]),std::abs(ws[s][j]-wv[s])));
        if(median(deviations)>.20) {pv[s]=wv[s]=missing;continue;}
        valid[s]=true;++count;
    }
    if(!coverage(valid)) return false;
    std::array<double,3> pf{},wf{};
    if(!harmonic(pv,pf) || !harmonic(wv,wf)) return false;
    double residual=0,mp=0,mw=0;
    for(int s=0;s<16;++s) if(valid[s]) {
        const double ct=std::cos(2*pi*s/16),st=std::sin(2*pi*s/16);
        const double ep=pv[s]-pf[0]-pf[1]*ct-pf[2]*st, ew=wv[s]-wf[0]-wf[1]*ct-wf[2]*st;
        residual+=ep*ep+ew*ew;mp+=pv[s]/count;mw+=wv[s]/count;
    }
    const double rms=std::sqrt(residual/(2*count));
    if(rms>.18) return false;
    double cov=0,vp=0,vw=0;
    for(int s=0;s<16;++s) if(valid[s]) {double a=pv[s]-mp,b=wv[s]-mw;cov+=a*b;vp+=a*a;vw+=b*b;}
    const double values[]={m.phase,c.x,c.y,m.spacing/median(widths),pf[1],pf[2],wf[1],wf[2],
        std::hypot(pf[1],pf[2]),direction(pf[1],pf[2]),std::hypot(wf[1],wf[2]),direction(wf[1],wf[2]),
        vp>1e-12 && vw>1e-12 ? std::clamp(cov/std::sqrt(vp*vw),-1.0,1.0) : missing,
        static_cast<double>(rings),static_cast<double>(count),rms};
    std::copy_n(values,16,out);
    for(int s=0;s<16;++s) if(valid[s]) {
        sector[4*s]=pv[s];sector[4*s+1]=wv[s];sector[4*s+2]=static_cast<double>(ps[s].size());sector[4*s+3]=1;
    }
    return true;
}
} // namespace

int fp::analyzeAsymmetry(const unsigned short* image,int width,int height,const int* input,int* roiOut,double* out,double* sector) {
    const auto count=width>0 && height>0 ? static_cast<std::uint64_t>(width)*height : 0;
    const void* ptrs[]={image,input,roiOut,out,sector};
    const std::size_t sizes[]={count<=INT_MAX ? static_cast<std::size_t>(count)*2 : 0,3*sizeof(int),3*sizeof(int),16*sizeof(double),64*sizeof(double)};
    for(int i=0;i<5;++i) for(int j=i+1;j<5;++j) if(overlaps(ptrs[i],sizes[i],ptrs[j],sizes[j])) return -1;
    if(roiOut) std::fill_n(roiOut,3,0);
    if(out) std::fill_n(out,16,missing);
    if(sector) for(int s=0;s<16;++s) {sector[4*s]=sector[4*s+1]=missing;sector[4*s+2]=sector[4*s+3]=0;}
    if(!image || !input || !roiOut || !out || !sector || !count || count>INT_MAX) return -1;
    Roi roi{input[0],input[1],input[2]};
    const bool automatic=roi.left==0 && roi.top==0 && roi.size==0;
    if(!automatic && (roi.left<0 || roi.top<0 || roi.size<=0 || roi.size>width || roi.size>height || roi.left>width-roi.size || roi.top>height-roi.size)) return -1;
    try {
        if(automatic && !detectRoi(analysisView(image,static_cast<std::size_t>(count)),width,height,roi)) return -3;
        roiOut[0]=roi.left;roiOut[1]=roi.top;roiOut[2]=roi.size;
        std::vector<unsigned short> crop(static_cast<std::size_t>(roi.size)*roi.size);
        for(int y=0;y<roi.size;++y) std::copy_n(image+static_cast<std::size_t>(y+roi.top)*width+roi.left,roi.size,crop.data()+static_cast<std::size_t>(y)*roi.size);
        return analyze(analysisView(crop.data(),crop.size()),roi.size,out,sector) ? 0 : -4;
    } catch(...) {return -4;}
}
