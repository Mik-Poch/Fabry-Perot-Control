extern "C" {
#include "FPAnalyzer.h"
}
#include "Analysis.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

static_assert(CHAR_BIT == 8 && sizeof(int) == 4 && sizeof(double) == 8,
              "FPAnalyzer requires U8, I32 and DBL ABI types");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE-754 is required");
static_assert(sizeof(unsigned short) == 2 && USHRT_MAX == 65535,
              "FPAnalyzer requires 16-bit unsigned short");
static_assert(std::numeric_limits<std::size_t>::max()/sizeof(unsigned short) >= INT_MAX,
              "U16 byte counts must fit size_t for every I32 pixel count");

namespace {
bool overlaps(const void* a, std::size_t na, const void* b, std::size_t nb) {
    const auto x = reinterpret_cast<std::uintptr_t>(a);
    const auto y = reinterpret_cast<std::uintptr_t>(b);
    // Subtraction rather than end-address addition avoids address overflow.
    return x <= y ? y - x < na : x - y < nb;
}
template<typename Pixel>
int analyzeFrame(
    const Pixel image_pixels[], int image_width, int image_height,
    Pixel cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[3], double metrics_out[4]) {
    // Reject overlapping storage before initialization to avoid corrupting input.
    if (image_width > 0 && image_height > 0 && cropped_capacity_pixels >= 0 &&
        image_pixels && cropped_pixels && roi_out && metrics_out) {
        const auto count = static_cast<std::uint64_t>(image_width) *
                           static_cast<std::uint64_t>(image_height);
        if (count <= static_cast<std::uint64_t>(INT_MAX)) {
            const void* ptrs[] = {image_pixels, cropped_pixels, roi_out, metrics_out};
            const std::size_t sizes[] = {static_cast<std::size_t>(count)*sizeof(Pixel),
                static_cast<std::size_t>(cropped_capacity_pixels)*sizeof(Pixel),
                3 * sizeof(int), 4 * sizeof(double)};
            for (int i = 0; i < 4; ++i)
                for (int j = i + 1; j < 4; ++j)
                    if (overlaps(ptrs[i], sizes[i], ptrs[j], sizes[j]))
                        return FP_INVALID_ARGUMENT;
        }
    }
    if (roi_out) std::fill_n(roi_out, 3, 0);
    if (metrics_out) std::fill_n(metrics_out, 4, std::numeric_limits<double>::quiet_NaN());
    if (!image_pixels || !cropped_pixels || !roi_out || !metrics_out ||
        image_width <= 0 || image_height <= 0 || cropped_capacity_pixels < 0)
        return FP_INVALID_ARGUMENT;
    // The capacity is I32; reject products which cannot be represented by it.
    const auto count = static_cast<std::uint64_t>(image_width) *
                       static_cast<std::uint64_t>(image_height);
    if (count > static_cast<std::uint64_t>(INT_MAX)) return FP_INVALID_ARGUMENT;

    try {
        auto view = fp::analysisView(image_pixels, static_cast<std::size_t>(count));
        fp::Roi roi;
        if (!fp::detectRoi(view, image_width, image_height, roi))
            return FP_NO_FRINGE_REGION;
        roi_out[0] = roi.left;
        roi_out[1] = roi.top;
        roi_out[2] = roi.size;
        const auto required = static_cast<std::size_t>(roi.size) * roi.size;
        if (required > static_cast<std::size_t>(cropped_capacity_pixels))
            return FP_INSUFFICIENT_CAPACITY;
        for (int y = 0; y < roi.size; ++y) {
            std::memcpy(cropped_pixels + static_cast<std::size_t>(y) * roi.size,
                        image_pixels + static_cast<std::size_t>(y + roi.top) * image_width + roi.left,
                        static_cast<std::size_t>(roi.size)*sizeof(Pixel));
        }
        view.pixels = cropped_pixels;
        fp::Center center;
        fp::Measurement measurement;
        if (!fp::estimateCenter(view, roi.size, center) ||
            !fp::measureProfile(fp::radialProfile(view, roi.size, center), measurement))
            return FP_INSUFFICIENT_RINGS;
        metrics_out[0] = measurement.phase;
        metrics_out[1] = center.x;
        metrics_out[2] = center.y;
        metrics_out[3] = measurement.finesse;
        return FP_SUCCESS;
    } catch (...) {
        // No C++ exceptions may cross the C/DLL boundary. A crop is valid only
        // after it was copied in full (copy itself cannot throw).
        if (roi_out[2] > 0 && static_cast<std::size_t>(roi_out[2]) * roi_out[2] >
                static_cast<std::size_t>(cropped_capacity_pixels))
            std::fill_n(roi_out, 3, 0);
        return FP_INSUFFICIENT_RINGS;
    }
}
} // namespace

extern "C" FP_INTERNAL_EXPORT int FP_INTERNAL_CALL FP_GetApiVersion(void) { return 1; }

extern "C" FP_INTERNAL_EXPORT int FP_INTERNAL_CALL FP_AnalyzeFrameU8(
    const unsigned char image_pixels[], int image_width, int image_height,
    unsigned char cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[3], double metrics_out[4]) {
    return analyzeFrame(image_pixels, image_width, image_height, cropped_pixels,
                        cropped_capacity_pixels, roi_out, metrics_out);
}

extern "C" FP_INTERNAL_EXPORT int FP_INTERNAL_CALL FP_AnalyzeFrameU16(
    const unsigned short image_pixels[], int image_width, int image_height,
    unsigned short cropped_pixels[], int cropped_capacity_pixels,
    int roi_out[], double metrics_out[]) {
    return analyzeFrame(image_pixels, image_width, image_height, cropped_pixels,
                        cropped_capacity_pixels, roi_out, metrics_out);
}
