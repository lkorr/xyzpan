#pragma once
#include <array>
#include <cmath>

namespace xyzpan::dsp {

class SineLUT {
public:
    static constexpr int kSize = 2048;

    // Lookup sin(phase01 * 2*PI) where phase01 is in [0, 1).
    // Uses linear interpolation between table entries.
    static float lookup(float phase01) {
        const float idx = phase01 * kSize;
        const int i0 = static_cast<int>(idx) & (kSize - 1);
        const int i1 = (i0 + 1) & (kSize - 1);
        const float frac = idx - static_cast<float>(static_cast<int>(idx));
        return table_[i0] + frac * (table_[i1] - table_[i0]);
    }

    // Lookup cos(phase01 * 2*PI) = sin((phase01 + 0.25) * 2*PI).
    static float cosLookup(float phase01) {
        return lookup(phase01 + 0.25f);
    }

    // Lookup sin(angleRadians) for arbitrary angle in [-PI, PI] or beyond.
    // Normalizes to [0, 1) phase internally.
    static float lookupAngle(float angleRadians) {
        constexpr float kInvTwoPI = 1.0f / 6.28318530f;
        float phase = angleRadians * kInvTwoPI;
        phase -= static_cast<float>(static_cast<int>(phase));  // fract
        if (phase < 0.0f) phase += 1.0f;
        return lookup(phase);
    }

    // Lookup cos(angleRadians) for arbitrary angle.
    static float cosLookupAngle(float angleRadians) {
        constexpr float kInvTwoPI = 1.0f / 6.28318530f;
        float phase = angleRadians * kInvTwoPI + 0.25f;
        phase -= static_cast<float>(static_cast<int>(phase));
        if (phase < 0.0f) phase += 1.0f;
        return lookup(phase);
    }

private:
    static inline const std::array<float, kSize> table_ = []() {
        std::array<float, kSize> t{};
        for (int i = 0; i < kSize; ++i)
            t[i] = std::sin(2.0f * 3.14159265358979f * static_cast<float>(i) / kSize);
        return t;
    }();
};

} // namespace xyzpan::dsp
