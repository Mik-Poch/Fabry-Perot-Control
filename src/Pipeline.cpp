extern "C" {
#include "FPAnalyzer.h"
}
#include "Pipeline.h"
#include "Methods.h"
#include <chrono>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>

static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && sizeof(double) == 8,
              "FPAnalyzer requires I32 and DBL ABI types");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE-754 is required");
static_assert(sizeof(unsigned short) == 2 && USHRT_MAX == 65535,
              "FPAnalyzer requires 16-bit unsigned short");
static_assert(std::numeric_limits<std::size_t>::max()/sizeof(unsigned short) >= INT_MAX,
              "U16 byte counts must fit size_t for every I32 pixel count");

namespace {
bool overlaps(const void* a, std::size_t na, const void* b, std::size_t nb) {
    if (!a || !b || na == 0 || nb == 0) return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a);
    const auto y = reinterpret_cast<std::uintptr_t>(b);
    // Subtraction rather than end-address addition avoids address overflow.
    return x <= y ? y - x < na : x - y < nb;
}
} // namespace

int fp::analyzeFrame(
    const unsigned short image_pixels[], int image_width, int image_height,
    const int roi_in[3], int method, unsigned short cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[3], double metrics_out[4], Diagnostics* diagnostics) {
    if (diagnostics) *diagnostics = {};
    const auto count = image_width > 0 && image_height > 0 ?
        static_cast<std::uint64_t>(image_width) * static_cast<std::uint64_t>(image_height) : 0;
    // Check all known storage ranges before initializing any outputs, including
    // roi_in/roi_out aliasing and partial-null calls. No raw-pointer length inference.
    const void* ptrs[] = {image_pixels, roi_in, cropped_pixels, roi_out, metrics_out};
    const std::size_t sizes[] = {
        count <= INT_MAX ? static_cast<std::size_t>(count)*sizeof(unsigned short) : 0,
        3*sizeof(int),
        cropped_capacity_pixels > 0 ? static_cast<std::size_t>(cropped_capacity_pixels)*sizeof(unsigned short) : 0,
        3*sizeof(int), 4*sizeof(double)};
    for (int i = 0; i < 5; ++i)
        for (int j = i + 1; j < 5; ++j)
            if (overlaps(ptrs[i], sizes[i], ptrs[j], sizes[j]))
                return FP_INVALID_ARGUMENT;

    if (roi_out) std::fill_n(roi_out, 3, 0);
    if (metrics_out) std::fill_n(metrics_out, 4, std::numeric_limits<double>::quiet_NaN());
    if (!image_pixels || !roi_in || !cropped_pixels || !roi_out || !metrics_out ||
        count == 0 || count > INT_MAX || cropped_capacity_pixels < 0)
        return FP_INVALID_ARGUMENT;

    if (method < 0 || method > 3) return FP_INVALID_METHOD;

    const bool automatic = roi_in[0] == 0 && roi_in[1] == 0 && roi_in[2] == 0;
    fp::Roi roi{roi_in[0], roi_in[1], roi_in[2]};
    if (!automatic && (roi.left < 0 || roi.top < 0 || roi.size <= 0 ||
        roi.size > image_width || roi.size > image_height ||
        roi.left > image_width-roi.size || roi.top > image_height-roi.size))
        return FP_INVALID_ARGUMENT; // Subtraction avoids signed coordinate overflow.

    bool cropCopied = false;
    try {
        StageClock stage(diagnostics);
        if (automatic) {
            // This full-frame pass is entirely absent from the manual path.
            const auto frameView = fp::analysisView(image_pixels, static_cast<std::size_t>(count));
            if (!fp::detectRoi(frameView, image_width, image_height, roi))
                return FP_NO_FRINGE_REGION;
        }
        stage.record(&Diagnostics::roiMs);
        const auto required = static_cast<std::size_t>(roi.size) * roi.size;
        roi_out[0] = roi.left;
        roi_out[1] = roi.top;
        roi_out[2] = roi.size;
        if (required > static_cast<std::size_t>(cropped_capacity_pixels))
            return FP_INSUFFICIENT_CAPACITY;
        for (int y = 0; y < roi.size; ++y) {
            std::memcpy(cropped_pixels + static_cast<std::size_t>(y)*roi.size,
                        image_pixels + static_cast<std::size_t>(y+roi.top)*image_width+roi.left,
                        static_cast<std::size_t>(roi.size)*sizeof(unsigned short));
        }
        cropCopied = true;
        stage.record(&Diagnostics::copyMs);
        // Identical normalization and numerical analysis for equivalent ROIs.
        const auto cropView = fp::analysisView(cropped_pixels, required);
        stage.record(&Diagnostics::normalizationMs);
        fp::Center center;
        fp::Measurement measurement;
        if (!fp::analyzeCrop(cropView, roi.size, method, center, measurement, diagnostics))
            return FP_INSUFFICIENT_RINGS;
        metrics_out[0] = measurement.phase;
        metrics_out[1] = center.x;
        metrics_out[2] = center.y;
        metrics_out[3] = measurement.finesse;
        return FP_SUCCESS;
    } catch (...) {
        // No C++ exception crosses the ABI. Geometry implies a complete crop.
        if (!cropCopied) std::fill_n(roi_out, 3, 0);
        return FP_INSUFFICIENT_RINGS;
    }
}
