#include "Methods.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace fp {
namespace {
constexpr double pi=3.14159265358979323846;
double wrap(double value) { return value-std::floor(value); }
double median(std::vector<double> values) {
    if(values.empty()) return 0;
    const auto k=values.size()/2;
    std::nth_element(values.begin(),values.begin()+static_cast<std::ptrdiff_t>(k),values.end());
    return values[k];
}
bool solve(std::vector<double> matrix,std::vector<double> rhs,std::vector<double>& result) {
    const int n=static_cast<int>(rhs.size());
    for(int k=0;k<n;++k) {
        int pivot=k;
        for(int i=k+1;i<n;++i) if(std::abs(matrix[i*n+k])>std::abs(matrix[pivot*n+k])) pivot=i;
        if(!std::isfinite(matrix[pivot*n+k]) || std::abs(matrix[pivot*n+k])<1e-14) return false;
        for(int j=k;j<n;++j) std::swap(matrix[k*n+j],matrix[pivot*n+j]);
        std::swap(rhs[k],rhs[pivot]);
        for(int i=k+1;i<n;++i) {
            const double f=matrix[i*n+k]/matrix[k*n+k];
            for(int j=k+1;j<n;++j) matrix[i*n+j]-=f*matrix[k*n+j];
            rhs[i]-=f*rhs[k];
        }
    }
    result.assign(n,0);
    for(int i=n-1;i>=0;--i) {
        double value=rhs[i];
        for(int j=i+1;j<n;++j) value-=matrix[i*n+j]*result[j];
        result[i]=value/matrix[i*n+i];
        if(!std::isfinite(result[i])) return false;
    }
    return true;
}

bool reference(PixelView view,int size,Center& center,Measurement& result,Diagnostics* d,ProfileFit* fit=nullptr) {
    StageClock stage(d);
    if(!estimateCenter(view,size,center)) return false;
    stage.record(&Diagnostics::centerMs);
    auto profile=radialProfile(view,size,center);
    stage.record(&Diagnostics::profileMs);
    const bool ok=measureProfile(profile,result,fit);
    stage.record(&Diagnostics::peaksMs);
    return ok;
}

// The fit uses individual 2D pixels across the supported disk, not a 1D profile.
// A profile only initializes the parameters and excludes the aperture taper.
struct FitSample { double x,y,value; };
using Params=std::array<double,11>; // cx/size,cy/size,log P,phase,B,log A,log F,bx,by,ax,ay
struct AiryProblem {
    int size;
    Center origin;
    Params low{},high{};
    double evaluate(const std::vector<FitSample>& samples,const Params& p,
                    std::vector<double>* h=nullptr,std::vector<double>* g=nullptr) const {
        if(h) { h->assign(121,0); g->assign(11,0); }
        const double period=std::exp(p[2]), amplitude=std::exp(p[5]), coefficient=std::exp(p[6]);
        double cost=0;
        for(const auto& s:samples) {
            const double dx=s.x-p[0]*size,dy=s.y-p[1]*size;
            const double xx=(s.x-origin.x)/size,yy=(s.y-origin.y)/size;
            const double theta=pi*((dx*dx+dy*dy)/period+p[3]);
            const double sine=std::sin(theta),t=coefficient*sine*sine,den=1+t;
            const double light=amplitude*(1+p[9]*xx+p[10]*yy);
            const double model=p[4]+p[7]*xx+p[8]*yy+light/den;
            const double residual=s.value-model,absolute=std::abs(residual);
            constexpr double huber=.04;
            const double weight=absolute<=huber?1:huber/absolute;
            cost+=absolute<=huber?residual*residual:2*huber*absolute-huber*huber;
            if(h) {
                const double dt=-light*coefficient*std::sin(2*theta)/(den*den);
                const double jac[11]={dt*(-2*pi*dx/period*size),dt*(-2*pi*dy/period*size),
                    dt*(-pi*(dx*dx+dy*dy)/period),dt*pi,1,light/den,-light*t/(den*den),
                    xx,yy,amplitude*xx/den,amplitude*yy/den};
                for(int i=0;i<11;++i) {
                    (*g)[i]+=weight*jac[i]*residual;
                    for(int j=0;j<=i;++j) (*h)[i*11+j]+=weight*jac[i]*jac[j];
                }
            }
        }
        if(h) for(int i=0;i<11;++i) for(int j=0;j<i;++j) (*h)[j*11+i]=(*h)[i*11+j];
        return cost;
    }
    bool optimize(const std::vector<FitSample>& samples,Params& p,int& iterations) const {
        double lambda=1e-3;
        bool converged=false;
        for(int iteration=0;iteration<45;++iteration) {
            ++iterations;
            std::vector<double> h,g,step;
            const double cost=evaluate(samples,p,&h,&g);
            double gradient=0;
            for(double v:g) gradient=std::max(gradient,std::abs(v)/samples.size());
            if(gradient<1e-7) { converged=true;break; }
            for(int i=0;i<11;++i) h[i*11+i]+=lambda*(h[i*11+i]+1e-8);
            if(!solve(h,g,step)) return false;
            Params candidate=p;
            double change=0;
            for(int i=0;i<11;++i) {
                candidate[i]=std::clamp(p[i]+step[i],low[i],high[i]);
                change=std::max(change,std::abs(candidate[i]-p[i]));
            }
            const double next=evaluate(samples,candidate);
            if(std::isfinite(next) && next<cost) {
                p=candidate;lambda=std::max(1e-9,lambda*.3);
                if(change<1e-6 || cost-next<1e-9*std::max(1.0,cost)) { converged=true;break; }
            } else {
                lambda*=10;
                if(lambda>1e10) break;
            }
        }
        return converged;
    }
};
bool airy2D(PixelView view,int size,Center& center,Measurement& result,Diagnostics* d) {
    ProfileFit peaks;
    if(!reference(view,size,center,result,d,&peaks)) return false;
    StageClock stage(d);
    const double radius=std::min({center.x,center.y,size-1-center.x,size-1-center.y})-2;
    const double maxQ=std::min(radius*radius,peaks.peakQ.back()-.15*result.spacing);
    if(maxQ<2*result.spacing) return false;
    std::vector<FitSample> samples;
    const int fineStride=std::max(1,size/180),coarseStride=std::max(1,size/90);
    std::vector<double> values;
    for(int y=0;y<size;y+=fineStride) for(int x=0;x<size;x+=fineStride) {
        const double dx=x-center.x,dy=y-center.y;
        if(dx*dx+dy*dy<=maxQ) { samples.push_back({double(x),double(y),view[std::size_t(y)*size+x]});values.push_back(samples.back().value); }
    }
    if(samples.size()<300) return false;
    std::sort(values.begin(),values.end());
    const double low=values[values.size()/20],high=values[values.size()*995/1000];
    const double span=high-low;
    if(span<5) return false;
    for(auto& s:samples) s.value/=span;
    const double f=std::clamp(1/std::pow(std::sin(pi/(2*result.finesse)),2)-2,.2,300.0);
    Params p={center.x/size,center.y/size,std::log(result.spacing),result.phase,low/span,0,std::log(f),0,0,0,0};
    AiryProblem problem{size,center,{}, {}};
    problem.low={p[0]-4.0/size,p[1]-4.0/size,p[2]-.06,p[3]-.2,-2,-3,std::log(.1),-2,-2,-1,-1};
    problem.high={p[0]+4.0/size,p[1]+4.0/size,p[2]+.06,p[3]+.2,2,3,std::log(500.0),2,2,1,1};
    std::vector<FitSample> coarse;
    for(const auto& s:samples) if(int(s.x)%coarseStride==0 && int(s.y)%coarseStride==0) coarse.push_back(s);
    // Avoid a sparse intersection between relatively prime coarse/fine grids.
    if(coarse.size()<300) coarse=samples;
    int iterations=0;
    if(!problem.optimize(coarse,p,iterations) || !problem.optimize(samples,p,iterations)) return false;
    const double relativeRms=std::sqrt(problem.evaluate(samples,p)/samples.size());
    if(d) {d->fitRelativeRms=relativeRms;d->fitIterations=iterations;}
    if(relativeRms>.12 || std::exp(p[5])<.15) return false;
    for(int i : {0,1,2,3,6}) if(p[i]<=problem.low[i]+1e-6 || p[i]>=problem.high[i]-1e-6) return false;
    center={p[0]*size,p[1]*size};
    result.phase=wrap(p[3]);result.spacing=std::exp(p[2]);
    result.finesse=pi/(2*std::asin(1/std::sqrt(std::exp(p[6])+2)));
    stage.record(&Diagnostics::fitMs);
    return std::isfinite(result.finesse) && result.finesse>1;
}
// Sector peak shifts obey q_s = P*n - P*phase - |delta|^2
//                         + 2*sqrt(q_s)*(delta dot u_s).
// This fits signed center displacement and a shared order intercept without
// averaging wrapped phases. u_s is the angular mean of each 45-degree sector.
bool sectors(PixelView view,int size,Center& center,Measurement& result,Diagnostics* d) {
    if(!reference(view,size,center,result,d)) return false;
    StageClock stage(d);
    for(int iteration=0;iteration<2;++iteration) {
        if(d) { d->sectorValid.fill(false);d->sectorPhase.fill(std::numeric_limits<double>::quiet_NaN()); }
        const double radius=std::min({center.x,center.y,size-1-center.x,size-1-center.y})-1;
        const double step=std::max(1.0,radius*radius/4096);
        const int bins=int(std::floor(radius*radius/step))+1;
        std::array<Profile,8> profiles;
        std::array<std::vector<double>,8> counts;
        for(int s=0;s<8;++s) { profiles[s].step=step;profiles[s].intensity.assign(bins,0);counts[s].assign(bins,0); }
        for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
            const double dx=x-center.x,dy=y-center.y,q=(dx*dx+dy*dy)/step;
            if(q>=bins-1) continue;
            int s;
            if(std::abs(dy)<=.414213562373095*std::abs(dx)) s=dx>=0?0:4;
            else if(std::abs(dx)<=.414213562373095*std::abs(dy)) s=dy>=0?2:6;
            else s=dx>=0?(dy>=0?1:7):(dy>=0?3:5);
            const int i=int(q);const double f=q-i,v=view[std::size_t(y)*size+x];
            profiles[s].intensity[i]+=(1-f)*v;profiles[s].intensity[i+1]+=f*v;
            counts[s][i]+=1-f;counts[s][i+1]+=f;
        }
        stage.record(&Diagnostics::profileMs);
        struct Observation { std::array<double,4> x;double q;int sector; };
        std::vector<Observation> observations;
        std::vector<double> finesse;
        int valid=0;
        for(int s=0;s<8;++s) {
            auto& profile=profiles[s];
            for(int i=0;i<bins;++i) profile.intensity[i]=counts[s][i]>0?profile.intensity[i]/counts[s][i]:(i>0?profile.intensity[i-1]:0);
            const auto raw=profile.intensity;
            for(int i=1;i<bins-1;++i) profile.intensity[i]=(raw[i-1]+2*raw[i]+raw[i+1])/4;
            Measurement m;ProfileFit peaks;
            if(!measureProfile(profile,m,&peaks) || std::abs(m.spacing/result.spacing-1)>.08) continue;
            const double phaseDifference=std::remainder(m.phase-result.phase,1.0);
            if(std::abs(phaseDifference)>.15) continue;
            ++valid;finesse.push_back(m.finesse);
            if(d) { d->sectorPhase[s]=m.phase;d->sectorValid[s]=true; }
            const double mean=std::sin(pi/8)/(pi/8),ux=std::cos(s*pi/4)*mean,uy=std::sin(s*pi/4)*mean;
            for(double q:peaks.peakQ) {
                const double order=std::round(q/result.spacing+result.phase);
                observations.push_back({{order,1,2*std::sqrt(q)*ux/size,2*std::sqrt(q)*uy/size},q,s});
            }
        }
        stage.record(&Diagnostics::peaksMs);
        if(d) d->validSectors=valid;
        if(valid<5 || observations.size()<15) return false;
        std::vector<double> coefficients={result.spacing,-result.phase*result.spacing,0,0};
        for(int robust=0;robust<4;++robust) {
            std::vector<double> h(16,0),g(4,0);
            for(const auto& o:observations) {
                double predicted=0;for(int i=0;i<4;++i) predicted+=o.x[i]*coefficients[i];
                const double weight=std::min(1.0,.02*result.spacing/std::max(1e-12,std::abs(o.q-predicted)));
                for(int i=0;i<4;++i) {
                    g[i]+=weight*o.x[i]*o.q;
                    for(int j=0;j<4;++j) h[i*4+j]+=weight*o.x[i]*o.x[j];
                }
            }
            if(!solve(h,g,coefficients)) return false;
        }
        const double dx=coefficients[2]/size,dy=coefficients[3]/size;
        if(std::hypot(dx,dy)>2 || std::abs(coefficients[0]/result.spacing-1)>.06) return false;
        double rss=0;std::array<int,8> support{};
        if(d) d->sectorResidual.fill(0);
        for(const auto& o:observations) {
            double predicted=0;for(int i=0;i<4;++i) predicted+=o.x[i]*coefficients[i];
            const double residual=(o.q-predicted)/coefficients[0];rss+=residual*residual;
            if(d) { d->sectorResidual[o.sector]+=residual;++support[o.sector]; }
        }
        if(std::sqrt(rss/observations.size())>.04) return false;
        if(d) for(int s=0;s<8;++s) d->sectorResidual[s]=support[s]?d->sectorResidual[s]/support[s]:std::numeric_limits<double>::quiet_NaN();
        center.x+=dx;center.y+=dy;
        result.spacing=coefficients[0];result.phase=wrap(-(coefficients[1]+dx*dx+dy*dy)/result.spacing);
        result.finesse=median(finesse);
        stage.record(&Diagnostics::fitMs);
    }
    return result.finesse>1;
}

double sample(PixelView view,int size,double x,double y) {
    x=std::clamp(x,0.0,double(size-1));y=std::clamp(y,0.0,double(size-1));
    const int ix=std::min(size-2,int(x)),iy=std::min(size-2,int(y));
    const double fx=x-ix,fy=y-iy;
    const auto i=std::size_t(iy)*size+ix;
    return (1-fy)*((1-fx)*view[i]+fx*view[i+1])+fy*((1-fx)*view[i+size]+fx*view[i+size+1]);
}
// Eight spokes, four opposite pairs; signed radial lag = 2*delta dot u.
// No full-image gradient-center estimator is called on this path.
bool spokeCenter(PixelView view,int size,Center& center) {
    if(size<48) return false;
    center={(size-1)/2.0,(size-1)/2.0};
    for(int iteration=0;iteration<3;++iteration) {
        const double radius=std::min({center.x,center.y,size-1-center.x,size-1-center.y})-2;
        const int n=int(radius);
        if(n<24) return false;
        const int maxLag=iteration==0?std::min(int(size*.35),n-24):(iteration==1?12:3);
        std::vector<double> plus(n),minus(n);
        std::vector<double> h(4,0),g(2,0),displacements;
        double minCorrelation=1;
        for(int pair=0;pair<4;++pair) {
            const double ux=std::cos(pair*pi/4),uy=std::sin(pair*pi/4);
            for(int i=0;i<n;++i) {
                plus[i]=sample(view,size,center.x+i*ux,center.y+i*uy);
                minus[i]=sample(view,size,center.x-i*ux,center.y-i*uy);
            }
            const auto correlation=[&](double lag) {
                double a=0,b=0,aa=0,bb=0,ab=0;int count=0;
                for(int i=3;i<n-2;++i) {
                    const double j=i-lag;
                    if(j<3 || j>=n-2) continue;
                    const int k=int(j);const double f=j-k;
                    const double v=plus[i],w=minus[k]*(1-f)+minus[k+1]*f;
                    a+=v;b+=w;aa+=v*v;bb+=w*w;ab+=v*w;++count;
                }
                if(count<std::max(24,n/3)) return -1.0;
                const double va=aa-a*a/count,vb=bb-b*b/count;
                if(va<count || vb<count) return -1.0;
                return (ab-a*b/count)/std::sqrt(va*vb);
            };
            int best=0;double bestScore=-1;
            std::vector<double> scores(2*maxLag+1);
            for(int lag=-maxLag;lag<=maxLag;++lag) {
                const double score=correlation(lag);scores[lag+maxLag]=score;
                if(score>bestScore) {bestScore=score;best=lag;}
            }
            if(bestScore<.65 || (iteration==0 && std::abs(best)==maxLag)) return false;
            if(iteration==0) {
                double competitor=-1;
                for(int lag=-maxLag;lag<=maxLag;++lag) if(std::abs(lag-best)>4) competitor=std::max(competitor,scores[lag+maxLag]);
                if(bestScore-competitor<.04) return false;
            }
            // Subpixel search evaluates interpolated profiles rather than a
            // parabola through potentially asymmetric undersampled peaks.
            double lag=best;
            for(double step : {.2,.04,.008}) {
                double next=lag;
                for(int k=-5;k<=5;++k) {
                    const double candidate=lag+k*step;
                    if(std::abs(candidate)>maxLag+.9) continue;
                    const double score=correlation(candidate);
                    if(score>bestScore) {bestScore=score;next=candidate;}
                }
                lag=next;
            }
            minCorrelation=std::min(minCorrelation,bestScore);
            const double shift=lag/2,weight=bestScore*bestScore;
            h[0]+=weight*ux*ux;h[1]+=weight*ux*uy;h[2]+=weight*ux*uy;h[3]+=weight*uy*uy;
            g[0]+=weight*ux*shift;g[1]+=weight*uy*shift;
            displacements.push_back(shift);
        }
        std::vector<double> delta;
        if(!solve(h,g,delta)) return false;
        double rss=0;
        for(int k=0;k<4;++k) {
            const double error=displacements[k]-delta[0]*std::cos(k*pi/4)-delta[1]*std::sin(k*pi/4);
            rss+=error*error;
        }
        if(std::sqrt(rss/4)>(iteration==0?2.0:.6) || minCorrelation<.65) return false;
        if(iteration==2 && std::hypot(delta[0],delta[1])>.25) return false;
        center.x+=delta[0];center.y+=delta[1];
        if(center.x<12 || center.y<12 || center.x>size-13 || center.y>size-13) return false;
    }
    return true;
}
bool spokes(PixelView view,int size,Center& center,Measurement& result,Diagnostics* d) {
    StageClock stage(d);
    if(!spokeCenter(view,size,center)) return false;
    stage.record(&Diagnostics::centerMs);
    auto profile=radialProfile(view,size,center,std::max(1,size/350));
    stage.record(&Diagnostics::profileMs);
    const bool ok=measureProfile(profile,result);
    stage.record(&Diagnostics::peaksMs);
    return ok;
}

} // namespace

bool analyzeCrop(PixelView view,int size,int method,Center& center,Measurement& result,Diagnostics* d) {
    if(method==0) return spokes(view,size,center,result,d);
    if(method==1) return reference(view,size,center,result,d);
    if(method==2) return airy2D(view,size,center,result,d);
    if(method==3) return sectors(view,size,center,result,d);
    return false;
}
} // namespace fp
