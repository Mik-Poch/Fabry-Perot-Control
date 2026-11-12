extern "C" {
#include "FPAnalyzer.h"
}
#include "Analysis.h"
#include "Synthetic.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
struct Result {
    int roi[3] = {};
    double metrics[4] = {};
    std::vector<unsigned char> crop;
};
Result analyze(const synthetic::Parameters& p, double centerTolerance = 0.7, double phaseTolerance = 0.025) {
    const auto image = synthetic::frame(p);
    Result r;
    r.crop.resize(image.size()+32,0xA5);
    const int status = FP_AnalyzeFrameU8(image.data(),p.width,p.height,r.crop.data(),
        static_cast<int>(image.size()),r.roi,r.metrics);
    std::cout << "  " << p.width << 'x' << p.height << " center " << p.cx << ',' << p.cy
              << " status=" << status << " ROI=" << r.roi[0] << ',' << r.roi[1] << ',' << r.roi[2]
              << " metrics=" << r.metrics[0] << ',' << r.metrics[1] << ',' << r.metrics[2] << ',' << r.metrics[3] << '\n';
    check(status == 0,"synthetic frame must succeed");
    const int left = r.roi[0],top = r.roi[1],size = r.roi[2];
    check(size > 0 && size <= std::min(p.width,p.height),"square ROI size");
    check(left >= 0 && top >= 0 && left+size <= p.width && top+size <= p.height,"ROI bounds");
    check(std::abs(left+r.metrics[1]-p.cx) < centerTolerance,"center X accuracy");
    check(std::abs(top+r.metrics[2]-p.cy) < centerTolerance,"center Y accuracy");
    check(r.metrics[0] >= 0 && r.metrics[0] < 1,"phase range");
    check(synthetic::phaseError(r.metrics[0],p.phase) < phaseTolerance,"phase accuracy");
    check(std::isfinite(r.metrics[3]) && r.metrics[3] > 1,"positive resolved finesse");
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
        check(r.crop[static_cast<std::size_t>(y)*size+x] == image[static_cast<std::size_t>(y+top)*p.width+x+left],"exact crop pixels");
    check(std::all_of(r.crop.begin()+static_cast<std::ptrdiff_t>(size)*size,r.crop.end(),
        [](unsigned char v){return v == 0xA5;}),"crop tail and canary untouched");
    return r;
}
void checkNaN(const double* m) {
    for (int i = 0; i < 4; ++i) check(std::isnan(m[i]),"invalid metrics must be NaN");
}
void invalidArguments() {
    unsigned char input[64] = {}, crop[64];
    std::fill_n(crop,64,0xA5);
    int roi[3]; double m[4];
    check(FP_GetApiVersion() == 1,"API version");
    // Products just above I32_MAX and at 2^32 must be rejected before reading
    // input, including when size_t and pointers are only 32 bits wide.
    for (const auto dims : {std::pair<int,int>{0,8},{-1,8},{8,0},{8,-1},
                           {46341,46341},{65536,65536},{INT_MAX,2},{INT_MAX,INT_MAX}}) {
        check(FP_AnalyzeFrameU8(input,dims.first,dims.second,crop,64,roi,m) == -1,"invalid dimensions");
        check(roi[2] == 0,"invalid ROI initialized"); checkNaN(m);
    }
    check(FP_AnalyzeFrameU8(nullptr,8,8,crop,64,roi,m) == -1,"null input");
    check(FP_AnalyzeFrameU8(input,8,8,nullptr,64,roi,m) == -1,"null crop");
    check(FP_AnalyzeFrameU8(input,8,8,crop,64,nullptr,m) == -1,"null ROI");
    check(FP_AnalyzeFrameU8(input,8,8,crop,64,roi,nullptr) == -1,"null metrics");
    check(FP_AnalyzeFrameU8(input,8,8,crop,-1,roi,m) == -1,"negative capacity");
    check(FP_AnalyzeFrameU8(input,8,8,input,64,roi,m) == -1,"overlap");
    check(FP_AnalyzeFrameU8(input,8,8,crop,64,roi,m) == -3,"tiny image has no ROI");
    check(std::all_of(crop,crop+64,[](unsigned char v){return v==0xA5;}),"invalid calls leave crop untouched");
}
void capacity() {
    synthetic::Parameters p;
    const auto image = synthetic::frame(p);
    unsigned char sentinel[8]; std::fill_n(sentinel,8,0xA5);
    int roi[3]; double m[4];
    check(FP_AnalyzeFrameU8(image.data(),p.width,p.height,sentinel+1,0,roi,m) == -2,"zero capacity");
    check(roi[2] > 0,"capacity failure returns geometry"); checkNaN(m);
    const int required = roi[2]*roi[2];
    std::vector<unsigned char> crop(static_cast<std::size_t>(required)+2,0xA5);
    check(FP_AnalyzeFrameU8(image.data(),p.width,p.height,crop.data()+1,required-1,roi,m) == -2,"one byte short");
    check(std::all_of(crop.begin(),crop.end(),[](unsigned char v){return v==0xA5;}),"no partial crop");
    check(FP_AnalyzeFrameU8(image.data(),p.width,p.height,crop.data()+1,required,roi,m) == 0,"exact capacity");
    check(crop.front() == 0xA5 && crop.back() == 0xA5,"crop guards");
}
void absentRings() {
    synthetic::Parameters p;
    std::vector<unsigned char> input(static_cast<std::size_t>(p.width)*p.height,0), crop(input.size(),0xA5);
    int roi[3]; double m[4];
    for (unsigned char value : {0,7,255}) {
        std::fill(input.begin(),input.end(),value);
        check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) == -3,"constant image");
        check(roi[2] == 0,"no-region ROI"); checkNaN(m);
    }
    std::fill(input.begin(),input.end(),5);
    input[input.size()/2] = 255;
    check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) == -3,"hot pixel is not rings");
    p.amplitude = 0; p.noise = 10; p.background = 40;
    input = synthetic::frame(p);
    check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) < 0,"noise is not rings");
    checkNaN(m);
    p.amplitude = 180; p.noise = 0; p.spacing = 100000; // fewer than three rings
    input = synthetic::frame(p);
    check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) == -4,"insufficient rings");
    checkNaN(m);
    check(roi[2] > 0,"measurement failure retains crop");
    for (int y = 0; y < roi[2]; ++y) for (int x = 0; x < roi[2]; ++x)
        check(crop[static_cast<std::size_t>(y)*roi[2]+x] == input[static_cast<std::size_t>(y+roi[1])*p.width+x+roi[0]],"failure crop valid");
    // Straight stripes have contrast but no finite common ring center.
    for (int y=0;y<p.height;++y) for (int x=0;x<p.width;++x)
        input[static_cast<std::size_t>(y)*p.width+x] = static_cast<unsigned char>(100+90*std::sin(x*0.15));
    check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) == -4,"stripes are not rings");
    // Structured contrast alone must not be accepted as a multi-ring pattern.
    for (int y=0;y<p.height;++y) for (int x=0;x<p.width;++x) {
        const double q = (x-p.cx)*(x-p.cx)+(y-p.cy)*(y-p.cy);
        input[static_cast<std::size_t>(y)*p.width+x] = static_cast<unsigned char>(5+200*std::exp(-q/8000));
    }
    check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) < 0,"Gaussian spot is not rings");
    p.amplitude = 0; p.background = 40; p.noise = 12;
    for (unsigned seed = 1; seed <= 8; ++seed) {
        p.seed = seed; input = synthetic::frame(p);
        check(FP_AnalyzeFrameU8(input.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,m) < 0,"noise seed rejected");
        checkNaN(m);
    }
}
void coreProfile() {
    fp::Profile profile;
    profile.step = 2.5;
    for (int i=0;i<3000;++i) {
        const double s = std::sin(synthetic::pi*(i*profile.step/1000+0.37));
        profile.intensity.push_back(10+180/(1+24*s*s));
    }
    fp::Measurement m;
    check(fp::measureProfile(profile,m),"analytic profile");
    check(synthetic::phaseError(m.phase,0.37)<0.001,"analytic profile phase");
    // Baseline is the adjacent Airy minimum, not zero; at this half height
    // sin^2(theta)=1/(2+airy), so finesse=pi/(2*asin(1/sqrt(airy+2))).
    const double expected = synthetic::pi/(2*std::asin(1/std::sqrt(26.0)));
    check(std::abs(m.finesse-expected) < 0.03*expected,"analytic profile FWHM");
    fp::Profile empty;
    check(!fp::measureProfile(empty,m),"empty profile");
    fp::Center center;
    std::vector<unsigned char> flat(64*64,8);
    check(!fp::estimateCenter(flat.data(),64,center),"flat center failure");
}
void frames() {
    synthetic::Parameters p;
    const auto narrow = analyze(p);
    p.airy = 5;
    const auto broad = analyze(p);
    check(narrow.metrics[3] > 1.7*broad.metrics[3],"broader rings reduce finesse");
    p.airy = 24;
    for (double phase : {0.005,0.23,0.73,0.995,1.27,-0.27}) { p.phase = phase; analyze(p); }
    p.phase = 0.27;
    for (const auto xy : {std::pair<double,double>{205.25,200.6},{430.6,265.2},{140.3,180.2},{495.2,310.4}}) {
        p.cx = xy.first; p.cy = xy.second; analyze(p);
    }
    p = {};
    p.width = 480; p.height = 640; p.cx = 238.4; p.cy = 321.1; analyze(p);
    p = {};
    p.width = 256; p.height = 256; p.cx = 124.6; p.cy = 127.2; p.radius = 110; p.spacing = 1800; analyze(p);
    p = {};
    p.amplitude = 28; p.noise = 2; p.background = 10; analyze(p,1.0,0.04);
    p = {};
    p.noise = 8; analyze(p,1.0,0.04);
    // A remote hot pixel must neither define the ROI nor pull the ring center.
    auto image = synthetic::frame(p);
    image[3*p.width+5] = 255;
    Result r; r.crop.resize(image.size());
    check(FP_AnalyzeFrameU8(image.data(),p.width,p.height,r.crop.data(),static_cast<int>(r.crop.size()),r.roi,r.metrics)==0,"hot pixel plus rings");
    check(std::abs(r.roi[0]+r.metrics[1]-p.cx)<1,"hot pixel center");
    p = {};
    p.width = 1280; p.height = 960; p.cx = 350.7; p.cy = 601.3;
    p.radius = 110; p.spacing = 1800; analyze(p);
}
void timing() {
    synthetic::Parameters p;
    p.width = 1280; p.height = 960; p.cx = 645.4; p.cy = 470.8;
    p.radius = 340; p.spacing = 15000; p.noise = 3;
    const auto image = synthetic::frame(p);
    std::vector<unsigned char> crop(image.size()); int roi[3]; double metrics[4];
    const auto begin = std::chrono::steady_clock::now();
    constexpr int repetitions = 3;
    for (int i=0;i<repetitions;++i)
        check(FP_AnalyzeFrameU8(image.data(),p.width,p.height,crop.data(),static_cast<int>(crop.size()),roi,metrics)==0,"timing image succeeds");
    const auto end = std::chrono::steady_clock::now();
    std::cout << "TIMING 1280x960 mean_ms=" << std::chrono::duration<double,std::milli>(end-begin).count()/repetitions
              << " repetitions=" << repetitions << '\n';
    check(std::abs(roi[0]+metrics[1]-p.cx)<1,"timing center");
    check(synthetic::phaseError(metrics[0],p.phase)<0.03,"timing phase");
}
}

int main() {
    int passed = 0,failed = 0;
    const std::pair<const char*,void(*)()> tests[] = {
        {"invalid arguments",invalidArguments},{"capacity and guards",capacity},
        {"absent or insufficient rings",absentRings},{"core profile",coreProfile},
        {"synthetic frames",frames},{"1280x960 timing",timing}};
    for (const auto& test : tests) {
        try { test.second(); ++passed; std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << test.first << ": " << e.what() << '\n'; }
    }
    std::cout << "groups passed=" << passed << " failed=" << failed << '\n';
    return failed ? 1 : 0;
}
