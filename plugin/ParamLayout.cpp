#include "ParamLayout.h"
#include "ParamIDs.h"
#include "xyzpan/Constants.h"

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

    // Phase 6: R scale/radius — multiplies XYZ before engine (PARAM-01)
    // Default 1.0 = unit radius (no scaling). Range 0.0–2.0 allows zoom-in and zoom-out.
    layout.add(std::make_unique<APF>(
        PID{ ParamID::R, 1 },
        "R Scale",
        NR(0.0f, 2.0f, 0.001f),
        1.0f  // Default: unit radius (no scaling)
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

    // -------------------------------------------------------------------------
    // Dev Panel: Comb filter bank (Phase 3) — DEPTH-05
    // Per-filter delays and feedback gains for the series comb bank.
    // Defaults from Constants.h (kCombDefaultDelays_ms / kCombDefaultFeedback).
    // -------------------------------------------------------------------------
    for (int i = 0; i < xyzpan::kMaxCombFilters; ++i) {
        layout.add(std::make_unique<APF>(
            PID{ ParamID::COMB_DELAY[i], 1 },
            "Comb Delay " + juce::String(i) + " (ms)",
            NR(0.0f, 1.5f, 0.01f),
            xyzpan::kCombDefaultDelays_ms[i]
        ));
        layout.add(std::make_unique<APF>(
            PID{ ParamID::COMB_FB[i], 1 },
            "Comb FB " + juce::String(i),
            NR(-0.95f, 0.95f, 0.01f),
            xyzpan::kCombDefaultFeedback[i]
        ));
    }

    layout.add(std::make_unique<APF>(
        PID{ ParamID::COMB_WET_MAX, 1 },
        "Comb Wet Max",
        NR(0.0f, 1.0f, 0.01f),
        xyzpan::kCombMaxWet  // 0.30f
    ));

    // -------------------------------------------------------------------------
    // Dev Panel: Elevation filter tuning (Phase 3) — ELEV-05
    // Pinna notch/shelf, chest bounce, and floor bounce parameters.
    // -------------------------------------------------------------------------

    // Pinna notch center frequency (Hz)
    layout.add(std::make_unique<APF>(
        PID{ ParamID::PINNA_NOTCH_HZ, 1 },
        "Pinna Notch Hz",
        NR(1000.0f, 16000.0f, 1.0f, 0.3f),  // Skew 0.3 for log-like feel
        xyzpan::kPinnaNotchFreqHz  // 8000.0f
    ));

    // Pinna notch Q
    layout.add(std::make_unique<APF>(
        PID{ ParamID::PINNA_NOTCH_Q, 1 },
        "Pinna Notch Q",
        NR(0.1f, 10.0f, 0.01f),
        xyzpan::kPinnaNotchQ  // 2.0f
    ));

    // Pinna high shelf frequency (Hz)
    layout.add(std::make_unique<APF>(
        PID{ ParamID::PINNA_SHELF_HZ, 1 },
        "Pinna Shelf Hz",
        NR(1000.0f, 16000.0f, 1.0f, 0.3f),  // Skew 0.3 for log-like feel
        xyzpan::kPinnaShelfFreqHz  // 4000.0f
    ));

    // Chest bounce max delay (ms)
    layout.add(std::make_unique<APF>(
        PID{ ParamID::CHEST_DELAY_MS, 1 },
        "Chest Delay Max (ms)",
        NR(0.0f, 10.0f, 0.01f),
        xyzpan::kChestDelayMaxMs  // 2.0f
    ));

    // Chest bounce gain (dB) — negative for attenuation
    layout.add(std::make_unique<APF>(
        PID{ ParamID::CHEST_GAIN_DB, 1 },
        "Chest Gain (dB)",
        NR(-30.0f, 0.0f, 0.1f),
        xyzpan::kChestGainDb  // -8.0f
    ));

    // Floor bounce max delay (ms)
    layout.add(std::make_unique<APF>(
        PID{ ParamID::FLOOR_DELAY_MS, 1 },
        "Floor Delay Max (ms)",
        NR(0.0f, 50.0f, 0.1f),
        xyzpan::kFloorDelayMaxMs  // 20.0f
    ));

    // Floor bounce gain (dB) — negative for attenuation
    layout.add(std::make_unique<APF>(
        PID{ ParamID::FLOOR_GAIN_DB, 1 },
        "Floor Gain (dB)",
        NR(-30.0f, 0.0f, 0.1f),
        xyzpan::kFloorGainDb  // -5.0f
    ));

    // -------------------------------------------------------------------------
    // Dev Panel: Distance processing tuning (Phase 4) -- DIST-07
    // -------------------------------------------------------------------------

    layout.add(std::make_unique<APF>(
        PID{ ParamID::DIST_DELAY_MAX_MS, 1 },
        "Dist Delay Max (ms)",
        NR(0.0f, 300.0f, 1.0f),
        xyzpan::kDistDelayMaxMs  // 300.0f
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::DIST_SMOOTH_MS, 1 },
        "Dist Smooth (ms)",
        NR(1.0f, 200.0f, 1.0f),
        xyzpan::kDistSmoothMs   // 30.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamID::DOPPLER_ENABLED, 1 },
        "Doppler",
        true  // enabled by default
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::AIR_ABS_MAX_HZ, 1 },
        "Air Abs Max Hz",
        NR(1000.0f, 22000.0f, 100.0f, 0.3f),  // Skew 0.3 for log-like feel (Hz param convention)
        xyzpan::kAirAbsMaxHz  // 22000.0f
    ));

    layout.add(std::make_unique<APF>(
        PID{ ParamID::AIR_ABS_MIN_HZ, 1 },
        "Air Abs Min Hz",
        NR(500.0f, 22000.0f, 100.0f, 0.3f),  // Skew 0.3 (Hz param convention)
        xyzpan::kAirAbsMinHz  // 8000.0f
    ));

    // -------------------------------------------------------------------------
    // Phase 5: Reverb (VERB-03)
    // -------------------------------------------------------------------------
    layout.add(std::make_unique<APF>(PID{ParamID::VERB_SIZE, 1}, "Verb Size",
        NR(0.0f, 1.0f, 0.01f), xyzpan::kVerbDefaultSize));
    layout.add(std::make_unique<APF>(PID{ParamID::VERB_DECAY, 1}, "Verb Decay",
        NR(0.0f, 1.0f, 0.01f), xyzpan::kVerbDefaultDecay));
    layout.add(std::make_unique<APF>(PID{ParamID::VERB_DAMPING, 1}, "Verb Damping",
        NR(0.0f, 1.0f, 0.01f), xyzpan::kVerbDefaultDamping));
    layout.add(std::make_unique<APF>(PID{ParamID::VERB_WET, 1}, "Verb Wet",
        NR(0.0f, 1.0f, 0.01f), xyzpan::kVerbDefaultWet));
    layout.add(std::make_unique<APF>(PID{ParamID::VERB_PRE_DELAY, 1}, "Verb Pre-Delay Max (ms)",
        NR(0.0f, 100.0f, 0.5f), xyzpan::kVerbPreDelayMaxMs));

    // -------------------------------------------------------------------------
    // Phase 5: LFO — per axis (LFO-01 through LFO-05)
    // -------------------------------------------------------------------------

    // X axis
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_X_RATE, 1}, "LFO X Rate (Hz)",
        NR(xyzpan::kLFOMinRate, xyzpan::kLFOMaxRate, 0.001f, 0.3f), xyzpan::kLFODefaultRate));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_X_DEPTH, 1}, "LFO X Depth",
        NR(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_X_PHASE, 1}, "LFO X Phase",
        NR(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_X_WAVEFORM, 1}, "LFO X Waveform",
        NR(0.0f, 3.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_X_BEAT_DIV, 1}, "LFO X Beat Div",
        NR(0.125f, 8.0f, 0.125f), 1.0f));

    // Y axis
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Y_RATE, 1}, "LFO Y Rate (Hz)",
        NR(xyzpan::kLFOMinRate, xyzpan::kLFOMaxRate, 0.001f, 0.3f), xyzpan::kLFODefaultRate));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Y_DEPTH, 1}, "LFO Y Depth",
        NR(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Y_PHASE, 1}, "LFO Y Phase",
        NR(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Y_WAVEFORM, 1}, "LFO Y Waveform",
        NR(0.0f, 3.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Y_BEAT_DIV, 1}, "LFO Y Beat Div",
        NR(0.125f, 8.0f, 0.125f), 1.0f));

    // Z axis
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Z_RATE, 1}, "LFO Z Rate (Hz)",
        NR(xyzpan::kLFOMinRate, xyzpan::kLFOMaxRate, 0.001f, 0.3f), xyzpan::kLFODefaultRate));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Z_DEPTH, 1}, "LFO Z Depth",
        NR(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Z_PHASE, 1}, "LFO Z Phase",
        NR(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Z_WAVEFORM, 1}, "LFO Z Waveform",
        NR(0.0f, 3.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<APF>(PID{ParamID::LFO_Z_BEAT_DIV, 1}, "LFO Z Beat Div",
        NR(0.125f, 8.0f, 0.125f), 1.0f));

    // Tempo sync (shared across all axes) — AudioParameterBool
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamID::LFO_TEMPO_SYNC, 1}, "LFO Tempo Sync", false));

    return layout;
}
