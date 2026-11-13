#include "Analysis.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace fp {
namespace {
double quantile(std::vector<double> v, double fraction) {
    if (v.empty()) return 0;
    const auto k = static_cast<std::size_t>(fraction * static_cast<double>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

// Separable [1 2 1]/4 filter, with replicated edges.
std::vector<double> smooth(const std::vector<double>& in, int w, int h) {
    std::vector<double> tmp(in.size()), out(in.size());
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const auto i = static_cast<std::size_t>(y) * w + x;
        tmp[i] = (in[static_cast<std::size_t>(y) * w + std::max(0, x-1)] +
                  2*in[i] + in[static_cast<std::size_t>(y) * w + std::min(w-1, x+1)]) / 4;
    }
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const auto i = static_cast<std::size_t>(y) * w + x;
        out[i] = (tmp[static_cast<std::size_t>(std::max(0, y-1)) * w + x] +
                  2*tmp[i] + tmp[static_cast<std::size_t>(std::min(h-1, y+1)) * w + x]) / 4;
    }
    return out;
}
}

PixelView analysisView(const unsigned short* pixels, std::size_t count) {
    // Exact U16 histogram: no U8 conversion, assumed camera bit depth, or
    // full-frame floating-point allocation (important in a 32-bit process).
    std::vector<std::size_t> histogram(65536, 0);
    for (std::size_t i = 0; i < count; ++i) ++histogram[pixels[i]];
    const auto percentile = [&](std::size_t rank) {
        std::size_t sum = 0;
        for (std::size_t i = 0; i < histogram.size(); ++i) {
            sum += histogram[i];
            if (sum > rank) return static_cast<double>(i);
        }
        return 0.0;
    };
    double low = percentile((count-1)/100);
    double high = percentile((count-1)-((count-1)/100));
    // Retain sparse patterns when the central percentiles are both background.
    if (high <= low) { low = percentile(0); high = percentile(count-1); }
    // Put the robust span in reference analysis units, entirely in double.
    // Thus existing contrast/gradient/profile floors are relative to this
    // region's contrast rather than to 255, 65535 or an inferred ADC bit depth.
    return {pixels, low, high > low ? 200.0/(high-low) : 1.0};
}

bool detectRoi(PixelView pixels, int width, int height, Roi& roi) {
    if (width < 24 || height < 24) return false;
    const int scale = std::max(1, static_cast<int>(std::ceil(std::max(width, height) / 400.0)));
    const int w = (width - 1) / scale + 1, h = (height - 1) / scale + 1;
    std::vector<double> small(static_cast<std::size_t>(w)*h);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        double sum = 0;
        int n = 0;
        for (int yy = y*scale; yy < std::min(height, (y+1)*scale); ++yy)
            for (int xx = x*scale; xx < std::min(width, (x+1)*scale); ++xx) {
                sum += pixels[static_cast<std::size_t>(yy)*width + xx];
                ++n;
            }
        small[static_cast<std::size_t>(y)*w+x] = sum/n;
    }
    small = smooth(small, w, h);
    constexpr int tile = 8;
    const int tw = (w - 1)/tile + 1, th = (h - 1)/tile + 1;
    std::vector<double> contrast(static_cast<std::size_t>(tw)*th);
    for (int ty = 0; ty < th; ++ty) for (int tx = 0; tx < tw; ++tx) {
        double sum = 0, sum2 = 0;
        int n = 0;
        for (int y = ty*tile; y < std::min(h, (ty+1)*tile); ++y)
            for (int x = tx*tile; x < std::min(w, (tx+1)*tile); ++x) {
                const double v = small[static_cast<std::size_t>(y)*w+x];
                sum += v; sum2 += v*v; ++n;
            }
        contrast[static_cast<std::size_t>(ty)*tw+tx] = std::sqrt(std::max(0.0, sum2/n - (sum/n)*(sum/n)));
    }
    // Robust upper quantile avoids selecting a single hot pixel. Downsampling
    // and smoothing suppress sensor noise before local-contrast thresholding.
    std::vector<double> visibleContrast;
    for (double value : contrast) if (value >= 0.8) visibleContrast.push_back(value);
    if (visibleContrast.empty()) return false;
    const double signal = quantile(visibleContrast, 0.90);
    const double threshold = std::max(0.8, 0.24*signal);
    std::vector<unsigned char> active(contrast.size(), 0), visited(contrast.size(), 0);
    for (int y = 0; y < th; ++y) for (int x = 0; x < tw; ++x)
        if (contrast[static_cast<std::size_t>(y)*tw+x] >= threshold) {
            // One-tile dilation joins rings separated by dark inter-ring gaps.
            for (int yy = std::max(0,y-1); yy <= std::min(th-1,y+1); ++yy)
                for (int xx = std::max(0,x-1); xx <= std::min(tw-1,x+1); ++xx)
                    active[static_cast<std::size_t>(yy)*tw+xx] = 1;
        }
    double best = 0;
    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    for (int seed = 0; seed < tw*th; ++seed) {
        if (!active[seed] || visited[seed]) continue;
        std::vector<int> queue{seed};
        visited[seed] = 1;
        double score = 0;
        int count = 0, x0 = tw, y0 = th, x1 = 0, y1 = 0;
        for (std::size_t k = 0; k < queue.size(); ++k) {
            const int i = queue[k], x = i % tw, y = i / tw;
            if (contrast[i] >= threshold) {
                score += std::min(contrast[i], signal);
                ++count;
                x0 = std::min(x0, x); x1 = std::max(x1, x);
                y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
            for (int yy = std::max(0,y-1); yy <= std::min(th-1,y+1); ++yy)
                for (int xx = std::max(0,x-1); xx <= std::min(tw-1,x+1); ++xx) {
                    const int j = yy*tw+xx;
                    if (active[j] && !visited[j]) { visited[j] = 1; queue.push_back(j); }
                }
        }
        if (count >= 6 && x1-x0 >= 2 && y1-y0 >= 2 && score > best) {
            best = score; bx0 = x0; bx1 = x1; by0 = y0; by1 = y1;
        }
    }
    if (best == 0) return false;
    const double cx = (bx0+bx1+1)*tile*scale/2.0 - 0.5;
    const double cy = (by0+by1+1)*tile*scale/2.0 - 0.5;
    const int extent = std::max(bx1-bx0+1, by1-by0+1)*tile*scale;
    roi.size = std::min(std::min(width,height), static_cast<int>(std::ceil(extent*1.08)));
    roi.left = std::clamp(static_cast<int>(std::lround(cx - (roi.size-1)/2.0)), 0, width-roi.size);
    roi.top = std::clamp(static_cast<int>(std::lround(cy - (roi.size-1)/2.0)), 0, height-roi.size);
    return true;
}

bool estimateCenter(PixelView pixels, int size, Center& center) {
    if (size < 24) return false;
    std::vector<double> input(static_cast<std::size_t>(size)*size);
    for (std::size_t i = 0; i < input.size(); ++i) input[i] = pixels[i];
    const auto blurred = smooth(smooth(input, size, size), size, size);
    struct Gradient { double x, y, nx, ny, magnitude; };
    std::vector<Gradient> gradients;
    std::vector<double> magnitudes;
    // Bound fitting cost for high-resolution frames without changing geometry.
    const int stride = std::max(1,size/600);
    for (int y = 2; y < size-2; y += stride) for (int x = 2; x < size-2; x += stride) {
        const auto i = static_cast<std::size_t>(y)*size+x;
        const double gx = (blurred[i+1]-blurred[i-1])/2;
        const double gy = (blurred[i+size]-blurred[i-size])/2;
        const double m = std::hypot(gx,gy);
        if (m > 0) {
            gradients.push_back({static_cast<double>(x),static_cast<double>(y),gy/m,-gx/m,m});
            magnitudes.push_back(m);
        }
    }
    if (gradients.size() < 100) return false;
    const double high = quantile(magnitudes,0.90);
    const double cutoff = std::max(0.3, high*0.30);
    if (high < 0.4) return false;
    center = {(size-1)/2.0,(size-1)/2.0};
    for (int iteration = 0; iteration < 8; ++iteration) {
        double a = 0,b = 0,d = 0,u = 0,v = 0,total = 0, good = 0;
        const double tolerance = std::max(1.5, size*(iteration < 3 ? 0.06 : 0.015));
        for (const auto& g : gradients) {
            if (g.magnitude < cutoff) continue;
            const double residual = g.nx*(center.x-g.x) + g.ny*(center.y-g.y);
            double weight = std::pow(std::min(g.magnitude,high),2);
            total += weight;
            if (iteration > 0) {
                const double t = residual/tolerance;
                weight /= (1+t*t)*(1+t*t);
            }
            good += weight;
            const double rhs = g.nx*g.x+g.ny*g.y;
            a += weight*g.nx*g.nx; b += weight*g.nx*g.ny; d += weight*g.ny*g.ny;
            u += weight*g.nx*rhs; v += weight*g.ny*rhs;
        }
        const double determinant = a*d-b*b;
        if (determinant <= 1e-6*(a+d)*(a+d) || good < 0.15*total) return false;
        center = {(u*d-v*b)/determinant,(v*a-u*b)/determinant};
        if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
            center.x < 4 || center.y < 4 || center.x > size-5 || center.y > size-5)
            return false;
    }
    return true;
}

Profile radialProfile(PixelView pixels, int size, const Center& center) {
    // Only complete circles: edge truncation must not bias peak intensity/width.
    const double radius = std::min({center.x,center.y,size-1-center.x,size-1-center.y})-1;
    Profile result;
    if (radius < 8) return result;
    const double maxQ = radius*radius;
    result.step = std::max(1.0,maxQ/4096.0);
    const int bins = static_cast<int>(std::floor(maxQ/result.step))+1;
    std::vector<double> sums(bins,0), counts(bins,0);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const double dx = x-center.x,dy = y-center.y;
        const double q = (dx*dx+dy*dy)/result.step;
        if (q >= bins-1) continue;
        const int i = static_cast<int>(q);
        const double f = q-i, value = pixels[static_cast<std::size_t>(y)*size+x];
        sums[i] += (1-f)*value; counts[i] += 1-f;
        sums[i+1] += f*value; counts[i+1] += f;
    }
    result.intensity.resize(bins);
    for (int i = 0; i < bins; ++i) {
        if (counts[i] > 0) result.intensity[i] = sums[i]/counts[i];
        else result.intensity[i] = i > 0 ? result.intensity[i-1] : pixels[
            static_cast<std::size_t>(std::lround(center.y))*size+static_cast<std::size_t>(std::lround(center.x))];
    }
    const auto raw = result.intensity;
    for (int i = 1; i < bins-1; ++i)
        result.intensity[i] = (raw[i-1]+2*raw[i]+raw[i+1])/4;
    return result;
}

bool measureProfile(const Profile& profile, Measurement& result) {
    const auto& p = profile.intensity;
    const int n = static_cast<int>(p.size());
    if (n < 32 || profile.step <= 0) return false;
    const double range = quantile(p,0.98)-quantile(p,0.10);
    if (range < 2.0) return false;
    struct Peak { int index; double q; };
    std::vector<Peak> peaks;
    const int start = std::max(3,static_cast<int>(16/profile.step));
    for (int i = start; i < n-3; ++i) {
        if (!(p[i] > p[i-1] && p[i] >= p[i+1])) continue;
        double left = p[i],right = p[i];
        for (int j = i-1; j >= start-1; --j) {
            left = std::min(left,p[j]);
            if (p[j] > p[i]) break;
        }
        for (int j = i+1; j < n; ++j) {
            right = std::min(right,p[j]);
            if (p[j] > p[i]) break;
        }
        if (p[i]-std::max(left,right) < 0.22*range) continue;
        const double curvature = p[i-1]-2*p[i]+p[i+1];
        const double offset = curvature < 0 ? 0.5*(p[i-1]-p[i+1])/curvature : 0;
        peaks.push_back({i,(i+std::clamp(offset,-0.5,0.5))*profile.step});
    }
    if (peaks.size() < 3) return false;
    std::vector<double> gaps;
    for (std::size_t i = 1; i < peaks.size(); ++i) gaps.push_back(peaks[i].q-peaks[i-1].q);
    const double typical = quantile(gaps,0.5);
    if (typical < 12*profile.step) return false; // sampling must resolve linewidth
    // Require a run of at least three consecutive, evenly spaced peaks. Ignore
    // small-radius sampling artifacts or an isolated envelope edge outside it.
    std::size_t first = 0,last = 0,run = 0;
    for (std::size_t i = 0; i < gaps.size(); ++i) {
        if (std::abs(gaps[i]-typical) > 0.12*typical) run = i+1;
        else if (i+1-run > last-first) { first = run; last = i+1; }
    }
    if (last-first < 2) return false;
    const double count = static_cast<double>(last-first+1);
    const double meanI = (count-1)/2;
    double meanQ = 0;
    for (std::size_t i = first; i <= last; ++i) meanQ += peaks[i].q/count;
    double cov = 0,var = 0;
    for (std::size_t i = first; i <= last; ++i) {
        const double t = static_cast<double>(i-first)-meanI;
        cov += t*(peaks[i].q-meanQ); var += t*t;
    }
    const double spacing = cov/var, intercept = meanQ-spacing*meanI;
    if (!(spacing > 0)) return false;
    for (std::size_t i = first; i <= last; ++i)
        if (std::abs(peaks[i].q - (intercept+spacing*static_cast<double>(i-first))) > 0.06*spacing)
            return false;
    std::vector<double> widths;
    for (std::size_t k = first; k <= last; ++k) {
        const int i = peaks[k].index;
        const int lo = k > 0 ? peaks[k-1].index : std::max(0,i-static_cast<int>(spacing/profile.step));
        const int hi = k+1 < peaks.size() ? peaks[k+1].index : std::min(n-1,i+static_cast<int>(spacing/profile.step));
        const auto lmin = std::min_element(p.begin()+lo,p.begin()+i);
        const auto rmin = std::min_element(p.begin()+i+1,p.begin()+hi+1);
        const double baseline = (*lmin+*rmin)/2;
        if (p[i]-baseline < 0.22*range) continue;
        const double half = (p[i]+baseline)/2;
        int l = i,r = i;
        while (l > lo && p[l] > half) --l;
        while (r < hi && p[r] > half) ++r;
        if (l == i || r == i || p[l] > half || p[r] > half) continue;
        const double left = l+(half-p[l])/(p[l+1]-p[l]);
        const double right = r-1+(p[r-1]-half)/(p[r-1]-p[r]);
        const double width = (right-left)*profile.step;
        if (width >= 2*profile.step && width < 0.8*spacing) widths.push_back(width);
    }
    if (widths.size() < 2) return false;
    result.phase = -intercept/spacing;
    result.phase -= std::floor(result.phase);
    if (result.phase >= 1) result.phase = 0;
    result.finesse = spacing/quantile(widths,0.5);
    return std::isfinite(result.phase) && std::isfinite(result.finesse) && result.finesse > 0;
}
} // namespace fp
