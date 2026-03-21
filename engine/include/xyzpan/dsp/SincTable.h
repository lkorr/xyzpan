#pragma once
// SincTable.h
// Polyphase windowed-sinc filter bank for high-quality fractional delay interpolation.
//
// Design:
//   - 8 lobes, 16 taps per sub-filter (2 * lobes)
//   - 128 sub-filters (fractional delay resolution)
//   - Kaiser window (beta = 7.0, ~70 dB stopband attenuation)
//   - Coefficients normalized per sub-filter (sum = 1.0) for unity DC gain
//   - Single static table shared by all delay line instances (~8 KB)
//
// Usage:
//   float result = SincTable::interpolate(buffer, mask, basePos, fractionalDelay);

#include <array>
#include <cmath>

namespace xyzpan::dsp {

class SincTable {
public:
    static constexpr int kLobes      = 8;
    static constexpr int kTaps       = 2 * kLobes;   // 16
    static constexpr int kSubFilters = 128;
    static constexpr int kTableSize  = kSubFilters * kTaps;  // 2048

    // Look up the sub-filter for a given fractional delay and apply the FIR
    // dot product over the ring buffer.
    //
    //   buf        — ring buffer data
    //   mask       — power-of-2 bitmask (bufSize - 1)
    //   basePos    — writePos_ - 1 - integerDelay (same convention as Hermite)
    //   frac       — fractional part of delay [0, 1)
    //   activeTaps — how many taps to use (2, 4, 8, or 16). Uses the center
    //                taps of the 16-tap table; outer taps are skipped.
    static float interpolate(const float* buf, int mask, int basePos, float frac,
                             int activeTaps = kTaps) {
        // Sub-filter index: frac in [0,1) maps to [0, kSubFilters)
        const float fidx = frac * static_cast<float>(kSubFilters);
        const int idx = static_cast<int>(fidx);
        const int si = (idx < kSubFilters) ? idx : (kSubFilters - 1);
        const float* coeffs = &table_[si * kTaps];

        // When activeTaps < kTaps, use only the center taps.
        // skip = number of taps to skip on each side.
        const int taps = (activeTaps >= 2 && activeTaps <= kTaps) ? activeTaps : kTaps;
        const int skip = (kTaps - taps) / 2;

        // The full 16 taps are centered around the interpolation point.
        // Tap offsets: basePos - (kLobes-1) .. basePos + kLobes
        // i.e., basePos - 7 .. basePos + 8
        float sum = 0.0f;
        float coeffSum = 0.0f;
        const int startPos = basePos - (kLobes - 1);
        for (int t = skip; t < kTaps - skip; ++t) {
            const float c = coeffs[t];
            sum += buf[static_cast<size_t>((startPos + t) & mask)] * c;
            coeffSum += c;
        }
        // Re-normalize for unity DC gain with fewer taps
        if (coeffSum > 1e-6f)
            sum /= coeffSum;
        return sum;
    }

private:
    // Kaiser window: I0(beta * sqrt(1 - x^2)) / I0(beta)
    // I0 approximation via series expansion (sufficient for beta <= 10)
    static constexpr double besselI0(double x) {
        double sum = 1.0;
        double term = 1.0;
        const double x2 = x * x * 0.25;
        for (int k = 1; k < 20; ++k) {
            term *= x2 / (static_cast<double>(k) * static_cast<double>(k));
            sum += term;
            if (term < 1e-12 * sum) break;
        }
        return sum;
    }

    static inline const std::array<float, kTableSize> table_ = []() {
        constexpr double kBeta = 7.0;
        constexpr double kPI = 3.14159265358979323846;
        const double I0beta = besselI0(kBeta);

        std::array<float, kTableSize> t{};

        for (int s = 0; s < kSubFilters; ++s) {
            // Fractional delay for this sub-filter: s / kSubFilters
            const double frac = static_cast<double>(s) / static_cast<double>(kSubFilters);

            double sum = 0.0;
            for (int tap = 0; tap < kTaps; ++tap) {
                // Tap offset from basePos: tap - (kLobes - 1)
                // Sinc argument: distance from the ideal fractional position
                const double n = static_cast<double>(tap - (kLobes - 1)) - frac;

                // Windowed sinc
                double sinc;
                if (std::abs(n) < 1e-10)
                    sinc = 1.0;
                else
                    sinc = std::sin(kPI * n) / (kPI * n);

                // Kaiser window position: n is in range [-(kLobes-1)-frac, kLobes-frac]
                // Normalize to [-1, 1] over the window span
                const double wpos = n / static_cast<double>(kLobes);
                double w;
                if (std::abs(wpos) >= 1.0)
                    w = 0.0;
                else
                    w = besselI0(kBeta * std::sqrt(1.0 - wpos * wpos)) / I0beta;

                t[static_cast<size_t>(s * kTaps + tap)] = static_cast<float>(sinc * w);
                sum += sinc * w;
            }

            // Normalize: ensure unity DC gain for this sub-filter
            if (std::abs(sum) > 1e-10) {
                const float invSum = static_cast<float>(1.0 / sum);
                for (int tap = 0; tap < kTaps; ++tap)
                    t[static_cast<size_t>(s * kTaps + tap)] *= invSum;
            }
        }

        return t;
    }();
};

} // namespace xyzpan::dsp
