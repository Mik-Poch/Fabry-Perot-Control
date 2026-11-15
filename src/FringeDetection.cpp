#include "FringeDetection.h"
#include <algorithm>
#include <cmath>
#include <vector>

int fp::findCenteredRoi(PixelView view,int width,int height,int requested,Roi& output) {
    Roi candidate;
    if(!detectRoi(view,width,height,candidate)) return -3;
    const double candidateX=candidate.left+(candidate.size-1)/2.0;
    const double candidateY=candidate.top+(candidate.size-1)/2.0;
    Center center;
    bool fitted=false;
    int previousSize=0;
    std::vector<unsigned short> temp;
    // All attempts stay around the detected region. Large dark frames must not
    // become the default center-fitting image. Reuse the workspace on retries.
    for(double factor:{1.0,1.5,2.0}) {
        const int side=std::min({width,height,std::max(24,static_cast<int>(std::ceil(candidate.size*factor)))});
        if(side==previousSize) break;
        previousSize=side;
        const int left=std::clamp(static_cast<int>(std::lround(candidateX-(side-1)/2.0)),0,width-side);
        const int top=std::clamp(static_cast<int>(std::lround(candidateY-(side-1)/2.0)),0,height-side);
        temp.resize(static_cast<std::size_t>(side)*side);
        for(int y=0;y<side;++y)
            std::copy_n(view.pixels+static_cast<std::size_t>(y+top)*width+left,side,temp.data()+static_cast<std::size_t>(y)*side);
        // Local normalization matches the proven crop-based center estimator.
        // No ring-count/prominence gate follows a successful geometric fit.
        if(estimateCenter(analysisView(temp.data(),temp.size()),side,center)) {
            center.x+=left;center.y+=top;fitted=true;break;
        }
    }
    if(!fitted) return -4;
    const double edge=std::min({center.x,center.y,width-1-center.x,height-1-center.y});
    const int maximum=std::min({width,height,static_cast<int>(std::floor(2*edge+1))});
    const double displacement=std::max(std::abs(center.x-candidateX),std::abs(center.y-candidateY));
    const double extent=candidate.size+2*displacement;
    const double padded=std::ceil(extent+2*std::max(4.0,.05*extent));
    const int automatic=static_cast<int>(std::min(static_cast<double>(maximum),std::max(24.0,padded)));
    const int side=requested ? requested : automatic;
    if(side<=0 || side>width || side>height || (!requested && side<24)) return requested ? -1 : -4;
    const int left=std::clamp(static_cast<int>(std::lround(center.x-(side-1)/2.0)),0,width-side);
    const int top=std::clamp(static_cast<int>(std::lround(center.y-(side-1)/2.0)),0,height-side);
    if(std::abs(center.x-left-(side-1)/2.0)>.75 || std::abs(center.y-top-(side-1)/2.0)>.75)
        return requested ? -1 : -4;
    output={left,top,side};
    return 0;
}
