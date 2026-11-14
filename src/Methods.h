#ifndef FP_METHODS_H
#define FP_METHODS_H
#include "Analysis.h"
#include "Pipeline.h"
namespace fp {
bool analyzeImage(PixelView,int,int,int,Center&,Measurement&,Diagnostics* = nullptr);
bool analyzeCrop(PixelView,int,int,Center&,Measurement&,Diagnostics* = nullptr);
}
#endif
