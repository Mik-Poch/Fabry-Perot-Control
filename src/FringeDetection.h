#ifndef FPF_DETECTION_H
#define FPF_DETECTION_H
#include "Analysis.h"
namespace fp {
// Geometry only inside the DLL; fitted center stays local to this operation.
int findCenteredRoi(PixelView,int,int,int,Roi&);
}
#endif
