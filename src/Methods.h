#ifndef FP_METHODS_H
#define FP_METHODS_H
#include "Analysis.h"
#include "Pipeline.h"
namespace fp {
bool analyzeCrop(PixelView,int,int,Center&,Measurement&,Diagnostics* = nullptr);
}
#endif
