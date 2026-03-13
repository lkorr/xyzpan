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
//   - read(delayInSamples) returns interpolated sample at a fractional position
//     in the past. 0.0 = most recent sample, positive = older.
//   - reset() zeros all state.
//   - Zero allocation in push/read/reset after prepare().
//
// Source: demofox.org/2015/08/08/cubic-hermite-interpolation/
//         (bitmask ring buffer pattern, standard audio DSP)

#include <vector>
#include <cmath>
#include <algorithm>

namespace xyzpan::dsp {

class FractionalDelayLine {
public:
    // Allocate the ring buffer with at least capacitySamples usable delay.
    // Internal size is next power-of-2 >= capacitySamples + 4 (Hermite lookahead).
    void prepare(int capacitySamples) {
        int n = 1;
        while (n < capacitySamples + 4) n <<= 1;
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

    // Read the buffer at a fractional delay past the most recent sample.
    //   delayInSamples == 0.0 returns the most recent pushed sample.
    //   delayInSamples == 1.0 returns the sample pushed before that, etc.
    //   Must satisfy: 0 <= delayInSamples < (buffer_size - 4).
    //
    // Cubic Hermite interpolation (Catmull-Rom).
    // Sample ordering (oldest to newest):
    //   A = sample at integer delay d+1  (one older than d)
    //   B = sample at integer delay d    (floor of requested delay)
    //   C = sample at integer delay d-1  (one newer than d)
    //   D = sample at integer delay d-2  (two newer than d, i.e., the very recent one)
    //
    // Wait — for a delay line, "older" samples are at larger index offsets from writePos_.
    // writePos_ - 1 is the most recently written sample.
    // writePos_ - 1 - d is the sample written d steps ago.
    //
    // We need 4 successive samples in time order (oldest to newest) straddling d:
    //   B = sample at position (writePos_ - 1 - d)         [integer delay d]
    //   C = sample at position (writePos_ - 1 - d + 1)     [newer by 1, delay d-1]
    //   A = sample at position (writePos_ - 1 - d - 1)     [older by 1, delay d+1]
    //   D = sample at position (writePos_ - 1 - d + 2)     [newer by 2, delay d-2]
    //
    // In Catmull-Rom convention: A=p[i-1], B=p[i], C=p[i+1], D=p[i+2]
    // where we interpolate between B and C at fraction t in [0,1).
    // B is the sample at integer delay d (floor of delayInSamples).
    // C is one sample newer (delay d-1), so t=0 gives B, t=1 gives C.
    //
    // Formula: ((a*t + b)*t + c)*t + B   (Horner's method)
    //   a = -0.5*A + 1.5*B - 1.5*C + 0.5*D
    //   b =       A - 2.5*B + 2.0*C - 0.5*D
    //   c = -0.5*A          + 0.5*C
    //
    float read(float delayInSamples) const {
        int d   = static_cast<int>(delayInSamples);
        float t = delayInSamples - static_cast<float>(d);

        // Base position: most recently written sample is at writePos_ - 1.
        int base = writePos_ - 1 - d;

        // A, B, C, D ordered oldest to newest in time
        // A = delay d+1 (one sample older than d)
        // B = delay d   (the integer-delay sample — interpolation anchor)
        // C = delay d-1 (one sample newer)
        // D = delay d-2 (two samples newer)
        float A = buf_[static_cast<size_t>((base - 1) & mask_)];
        float B = buf_[static_cast<size_t>((base    ) & mask_)];
        float C = buf_[static_cast<size_t>((base + 1) & mask_)];
        float D = buf_[static_cast<size_t>((base + 2) & mask_)];

        // Catmull-Rom / cubic Hermite coefficients
        float a = -0.5f*A + 1.5f*B - 1.5f*C + 0.5f*D;
        float b =        A - 2.5f*B + 2.0f*C - 0.5f*D;
        float c = -0.5f*A           + 0.5f*C;
        // d coefficient is B (constant term)

        return ((a * t + b) * t + c) * t + B;  // Horner's method
    }

private:
    std::vector<float> buf_;
    int mask_     = 0;
    int writePos_ = 0;
};

} // namespace xyzpan::dsp
