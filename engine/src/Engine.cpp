#include "xyzpan/Engine.h"
#include <algorithm>
#include <cstring>

namespace xyzpan {

void XYZPanEngine::prepare(double inSampleRate, int inMaxBlockSize) {
    sampleRate   = inSampleRate;
    maxBlockSize = inMaxBlockSize;
    monoBuffer.resize(static_cast<size_t>(inMaxBlockSize), 0.0f);
}

void XYZPanEngine::setParams(const EngineParams& params) {
    currentParams = params;
}

void XYZPanEngine::process(const float* const* inputs, int numInputChannels,
                            float* outL, float* outR, int numSamples) {
    // Phase 1: pass-through implementation.
    // Sum to mono if stereo input; copy mono to both output channels.

    if (inputs == nullptr || inputs[0] == nullptr || outL == nullptr || outR == nullptr)
        return;

    const float* monoIn = inputs[0];

    if (numInputChannels >= 2 && inputs[1] != nullptr) {
        // Sum stereo to mono (0.5 * (L + R))
        for (int i = 0; i < numSamples; ++i)
            monoBuffer[static_cast<size_t>(i)] = 0.5f * (inputs[0][i] + inputs[1][i]);
        monoIn = monoBuffer.data();
    }

    // Pass mono to both output channels
    for (int i = 0; i < numSamples; ++i) {
        outL[i] = monoIn[i];
        outR[i] = monoIn[i];
    }
}

void XYZPanEngine::reset() {
    // Phase 1: no-op. Future phases will reset delay lines and filter states here.
}

} // namespace xyzpan
