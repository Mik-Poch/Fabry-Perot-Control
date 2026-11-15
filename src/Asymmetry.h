#ifndef FP_ASYMMETRY_H
#define FP_ASYMMETRY_H
#include "Analysis.h"
namespace fp {
bool measureAsymmetry(PixelView,int,int,double*,double*);
int analyzeAsymmetry(const unsigned short*, int, int, const int*, int*, double*, double*);
}
#endif
