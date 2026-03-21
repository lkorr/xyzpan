#pragma once
// FractionalDelayLine.h
// Ring buffer with cubic Hermite (Catmull-Rom) interpolation for fractional
// delay positions. Used for ITD (Interaural Time Difference) in the binaural
// panning pipeline.
//
// Design:
//   - Power-of-2 buffer size with bitmask wraparound (never modulo).
//   - prepare() allocates and zeros the buffer.
//   - push() writes one sample at writePos_ and advances.
//   - read(delayInSamples) returns Hermite-interpolated sample (backward compat).
//   - read(delayInSamples, interpMode) dispatches to Hermite (0) or Sinc 2/4/8/16 (1-4).
//   - reset() zeros all state.
//   - Zero allocation in push/read/reset after prepare().
//
// Source: demofox.org/2015/08/08/cubic-hermite-interpolation/
//         (bitmask ring buffer pattern, standard audio DSP)

#include "xyzpan/dsp/SincTable.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

class FractionalDelayLine {
public:
    // Allocate the ring buffer with at least capacitySamples usable delay.
    // Internal size is next power-of-2 >= capacitySamples + padding.
    // Padding = 2 * SincTable::kLobes (16) to accommodate both Hermite (4) and Sinc (16).
    void prepare(int capacitySamples) {
        int n = 1;
        while (n < capacitySamples + 2 * SincTable::kLobes) n <<= 1;
        mask_     = n - 1;
        buf_.assign(static_cast<size_t>(n), 0.0f);
        writePos_ = 0;
    }

    // Zero all samples and reset write position.
    void reset() {
        std::fill(buf_.begin(), buf_.end(), 0.0f);
        writePos_ = 0;
    }

    // Write one sample into the ring buffer and advance the write pointer.
    void push(float sample) {
        buf_[static_cast<size_t>(writePos_ & mask_)] = sample;
        ++writePos_;
    }

    // Read the buffer at a fractional delay — backward-compatible Hermite path.
    //   delayInSamples == 0.0 returns the most recent pushed sample.
    //   Must satisfy: 0 <= delayInSamples < (buffer_size - 2*kLobes).
    float read(float delayInSamples) const {
        return readHermite(delayInSamples);
    }

    // Read with selectable interpolation mode.
    //   interpMode == 0 → Hermite (4-point cubic, fast)
    //   interpMode == 1 → Sinc 2-tap
    //   interpMode == 2 → Sinc 4-tap
    //   interpMode == 3 → Sinc 8-tap
    //   interpMode == 4 → Sinc 16-tap
    //   interpMode == 5 → ZOH (nearest neighbor — intentionally terrible)
    float read(float delayInSamples, int interpMode) const {
        if (interpMode == 5) return readZOH(delayInSamples);
        if (interpMode >= 1) {
            static constexpr int tapCounts[] = { 2, 4, 8, 16 };
            const int idx = std::clamp(interpMode - 1, 0, 3);
            return readSinc(delayInSamples, tapCounts[idx]);
        }
        return readHermite(delayInSamples);
    }

private:
    // Cubic Hermite interpolation (Catmull-Rom) — 4 taps.
    float readHermite(float delayInSamples) const {
        int d   = static_cast<int>(delayInSamples);
        float t = delayInSamples - static_cast<float>(d);

        // Invert fractional part so interpolation moves toward OLDER samples.
        // delay 5.7 → d=6, t=0.3: "at integer delay 6, 30% toward delay 5 (newer)"
        if (t > 0.0f) { t = 1.0f - t; d += 1; }

        int base = writePos_ - 1 - d;

        float A = buf_[static_cast<size_t>((base - 1) & mask_)];
        float B = buf_[static_cast<size_t>((base    ) & mask_)];
        float C = buf_[static_cast<size_t>((base + 1) & mask_)];
        float D = buf_[static_cast<size_t>((base + 2) & mask_)];

        float a = -0.5f*A + 1.5f*B - 1.5f*C + 0.5f*D;
        float b =        A - 2.5f*B + 2.0f*C - 0.5f*D;
        float c = -0.5f*A           + 0.5f*C;

        return ((a * t + b) * t + c) * t + B;
    }

    // Polyphase windowed-sinc interpolation — up to 16 taps.
    float readSinc(float delayInSamples, int activeTaps = 16) const {
        int d   = static_cast<int>(delayInSamples);
        float t = delayInSamples - static_cast<float>(d);

        // Invert fractional part so interpolation moves toward OLDER samples.
        if (t > 0.0f) { t = 1.0f - t; d += 1; }

        int base = writePos_ - 1 - d;

        return SincTable::interpolate(buf_.data(), mask_, base, t, activeTaps);
    }

    // Zero-order hold (nearest neighbor) — no interpolation at all.
    // Intentionally terrible: produces zipper noise and quantization artifacts.
    float readZOH(float delayInSamples) const {
        const int pos = (writePos_ - static_cast<int>(std::round(delayInSamples))) & mask_;
        return buf_[static_cast<size_t>(pos)];
    }

    std::vector<float> buf_;
    int mask_     = 0;
    int writePos_ = 0;
};

} // namespace xyzpan::dsp
