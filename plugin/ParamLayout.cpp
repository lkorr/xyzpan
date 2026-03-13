#include "ParamLayout.h"
#include "ParamIDs.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using APF = juce::AudioParameterFloat;
    using PID = juce::ParameterID;
    using NR  = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<APF>(
        PID{ ParamID::X, 1 },
        "X Position",
        NR(-1.0f, 1.0f, 0.001f),
        0.0f
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::Y, 1 },
        "Y Position",
        NR(-1.0f, 1.0f, 0.001f),
        1.0f  // Default: front (Y=1 in Y-forward convention)
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::Z, 1 },
        "Z Position",
        NR(-1.0f, 1.0f, 0.001f),
        0.0f
    ));

    // -------------------------------------------------------------------------
    // Dev Panel: Binaural panning tuning parameters (Phase 2)
    // These are hidden from the custom UI (Phase 6) but visible in the DAW's
    // generic editor for development/tuning purposes.
    // -------------------------------------------------------------------------

    // Dev Panel: ITD max — Woodworth spherical head model max delay
    layout.add(std::make_unique<APF>(
        PID{ ParamID::ITD_MAX_MS, 1 },
        "ITD Max (ms)",
        NR(0.0f, 5.0f, 0.01f),
        0.72f  // Default: Woodworth empirical max
    ));

    // Dev Panel: Head shadow — low-pass filter minimum cutoff (far ear at full azimuth)
    layout.add(std::make_unique<APF>(
        PID{ ParamID::HEAD_SHADOW_HZ, 1 },
        "Head Shadow Min Hz",
        NR(200.0f, 20000.0f, 1.0f, 0.3f),  // Skew 0.3 for log-like feel in generic editor
        1200.0f
    ));

    // Dev Panel: ILD max — maximum interaural level difference in dB
    layout.add(std::make_unique<APF>(
        PID{ ParamID::ILD_MAX_DB, 1 },
        "ILD Max (dB)",
        NR(0.0f, 24.0f, 0.1f),
        8.0f
    ));

    // Dev Panel: Rear shadow — both-ear LPF minimum cutoff when source is behind
    layout.add(std::make_unique<APF>(
        PID{ ParamID::REAR_SHADOW_HZ, 1 },
        "Rear Shadow Min Hz",
        NR(500.0f, 20000.0f, 1.0f, 0.3f),  // Skew 0.3 for log-like feel
        4000.0f
    ));

    // Dev Panel: Parameter smoothing time constants
    layout.add(std::make_unique<APF>(
        PID{ ParamID::SMOOTH_ITD_MS, 1 },
        "Smooth ITD (ms)",
        NR(1.0f, 50.0f, 0.1f),
        8.0f  // Slower to avoid Doppler pitch glitches
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::SMOOTH_FILTER_MS, 1 },
        "Smooth Filter (ms)",
        NR(1.0f, 50.0f, 0.1f),
        5.0f
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::SMOOTH_GAIN_MS, 1 },
        "Smooth Gain (ms)",
        NR(1.0f, 50.0f, 0.1f),
        5.0f
    ));

    return layout;
}
