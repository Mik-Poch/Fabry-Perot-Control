#ifndef FPF_DETECTION_H
#define FPF_DETECTION_H
#include "Analysis.h"
namespace fp {
bool checkFringes(PixelView,int,int);
int findCenteredRoi(PixelView,int,int,int,int*,double*);
}
#endif
