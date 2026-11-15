#ifndef FP_PIPELINE_H
#define FP_PIPELINE_H
#include <array>
#include <chrono>
namespace fp {
// Per-call diagnostics only, never exported and never global or thread_local.
struct Diagnostics {
    double roiMs=0, copyMs=0, normalizationMs=0, centerMs=0, profileMs=0, peaksMs=0, fitMs=0;
    double fitRelativeRms=0;
    int fitIterations=0, validSectors=0;
    std::array<double,8> sectorPhase{}, sectorResidual{};
    std::array<bool,8> sectorValid{};
};
class StageClock {
    Diagnostics* out;
    std::chrono::steady_clock::time_point last;
public:
    explicit StageClock(Diagnostics* d):out(d) { if(out) last=std::chrono::steady_clock::now(); }
    void record(double Diagnostics::* field) {
        if(out) { auto now=std::chrono::steady_clock::now(); out->*field+=std::chrono::duration<double,std::milli>(now-last).count();last=now; }
    }
};
int analyzeFrame(const unsigned short*,int,int,const int*,int,unsigned short*,int,int*,double*,Diagnostics* = nullptr);
}
#endif
