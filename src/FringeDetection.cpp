#include "FringeDetection.h"
#include <algorithm>
#include <cmath>
#include <vector>

int fp::findCenteredRoi(PixelView view,int width,int height,int requested,int* roiOut,double* centerOut) {
    Roi candidate;
    if(!detectRoi(view,width,height,candidate)) return -3;
    std::vector<unsigned short> temp(static_cast<std::size_t>(candidate.size)*candidate.size);
    for(int y=0;y<candidate.size;++y)
        std::copy_n(view.pixels+static_cast<std::size_t>(y+candidate.top)*width+candidate.left,candidate.size,temp.data()+static_cast<std::size_t>(y)*candidate.size);
    Center c;
    if(!estimateCenter({temp.data(),view.offset,view.gain},candidate.size,c)) return -4;
    c.x+=candidate.left;c.y+=candidate.top;
    // The ROI finder locates the fringe center; it is not a ring-count checker.
    // An extra full-frame radial evidence gate rejected valid, locally centered
    // fringe patterns (e.g. a bright inner ring with weaker outer rings).
    // Preserve the legacy ROI behavior: accept successful region localization
    // and robust local center fitting without imposing analysis-specific tests.
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
