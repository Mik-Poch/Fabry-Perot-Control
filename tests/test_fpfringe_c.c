#include "FPFringe.h"
#include <math.h>
#include <stdio.h>

typedef int (*Version)(void);
typedef int (*Find)(const unsigned short*,int,int,int,unsigned short*,int,int*);
typedef int (*Analyze)(const unsigned short*,int,int,int,double*);
typedef int (*Asymmetry)(const unsigned short*,int,int,double*,double*);
int main(void) {
    Version version=FPF_GetApiVersion;
    Find find=FPF_FindCenteredRoiU16;
    Analyze analyze=FPF_AnalyzeU16;
    Asymmetry asymmetry=FPF_AnalyzeAsymmetryU16;
    unsigned short image[32*48]={0};
    struct {int before,v[3],after;} roi={123,{0,0,0},456};
    struct {unsigned short before,value,after;} crop={123,60000,456};
    struct {double before,v[4],after;} m={123,{0,0,0,0},456};
    struct {double before,v[16],after;} a={123,{0},456};
    struct {double before,v[64],after;} s={123,{0},456};
    int i;
    if(version()!=3) return 1;
    if(find(image,48,32,0,&crop.value,1,roi.v)!=FPF_NO_FRINGE_REGION) return 2;
    if(analyze(image,48,32,1,m.v)!=FPF_UNRELIABLE_MEASUREMENT) return 4;
    if(asymmetry(image,48,32,a.v,s.v)!=FPF_UNRELIABLE_MEASUREMENT) return 5;
    for(i=0;i<3;++i) if(roi.v[i]!=0) return 6;
    for(i=0;i<4;++i) if(!isnan(m.v[i])) return 7;
    for(i=0;i<16;++i) if(!isnan(a.v[i]) || !isnan(s.v[4*i]) || !isnan(s.v[4*i+1]) || s.v[4*i+2]!=0 || s.v[4*i+3]!=0) return 8;
    if(roi.before!=123 || roi.after!=456 || crop.before!=123 || crop.after!=456 || crop.value!=60000 || m.before!=123 || m.after!=456 || a.before!=123 || a.after!=456 || s.before!=123 || s.after!=456) return 9;
    puts("PASS FPFringe plain C99: four pointer signatures, version, statuses, flat output canaries");
    return 0;
}
