#ifndef FP_ASYMMETRY_SYNTHETIC_H
#define FP_ASYMMETRY_SYNTHETIC_H
#include "Synthetic.h"
namespace asymSynthetic {
struct Parameters {
    int size=320,bits=16;
    double cx=153.3,cy=164.7,phase=.27,spacing=3600;
    double ax=0,ay=0,wx=0,wy=0,noise=0,background=.05,scale=.60;
    double ellipse=0,obscure=0,aperture=1e6;
    double illuminationX=0,illuminationY=0,backgroundX=0;
};
inline std::vector<unsigned short> frame(const Parameters& p) {
    std::vector<unsigned short> result(static_cast<std::size_t>(p.size)*p.size);
    std::mt19937 rng(57); std::normal_distribution<double> noise(0,1);
    const double maximum=(1<<p.bits)-1;
    for(int y=0;y<p.size;++y) for(int x=0;x<p.size;++x) {
        const double dx=x-p.cx,dy=y-p.cy,t=std::atan2(dy,dx),ct=std::cos(t),st=std::sin(t);
        const double q=(1+p.ellipse)*dx*dx+(1-p.ellipse)*dy*dy;
        // Exact trough-relative Airy FWHM_q = 0.24*P*(1+Wx*cos+Wy*sin).
        const double w=.24*(1+p.wx*ct+p.wy*st), s=std::sin(synthetic::pi*w/2);
        const double f=1/(s*s)-2, b=1/(1+f), a=std::sin(synthetic::pi*(q/p.spacing+p.phase));
        double shape=(1/(1+f*a*a)-b)/(1-b);
        if(p.obscure>0 && std::abs(t)<p.obscure) shape=0;
        const double taper=std::clamp((p.aperture-std::sqrt(q))/12.,0.,1.);
        double v=maximum*(p.background+p.backgroundX*dx/p.size+p.scale*(1+p.ax*ct+p.ay*st)*(1+p.illuminationX*dx/p.size+p.illuminationY*dy/p.size)*shape*taper)+p.noise*noise(rng);
        result[static_cast<std::size_t>(y)*p.size+x]=static_cast<unsigned short>(std::clamp(std::lround(v),0L,65535L));
    }
    return result;
}
}
#endif
