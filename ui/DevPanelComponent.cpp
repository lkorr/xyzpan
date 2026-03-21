#include "DevPanelComponent.h"
#include <cstdio>

// ParamID string constants duplicated to avoid including plugin headers.
// Must stay in sync with plugin/ParamIDs.h.
namespace {
    // Binaural
    constexpr const char* kITD_MAX_MS       = "itd_max_ms";
    constexpr const char* kHEAD_SHADOW_HZ   = "head_shadow_hz";
    constexpr const char* kILD_MAX_DB       = "ild_max_db";
    constexpr const char* kREAR_SHADOW_HZ   = "rear_shadow_hz";
    constexpr const char* kSMOOTH_ITD_MS    = "smooth_itd_ms";
    constexpr const char* kSMOOTH_FILTER_MS = "smooth_filter_ms";
    constexpr const char* kSMOOTH_GAIN_MS   = "smooth_gain_ms";

    // Comb
    constexpr const char* kCOMB_DELAY[10] = {
        "comb_delay_0", "comb_delay_1", "comb_delay_2", "comb_delay_3", "comb_delay_4",
        "comb_delay_5", "comb_delay_6", "comb_delay_7", "comb_delay_8", "comb_delay_9"
    };
    constexpr const char* kCOMB_FB[10] = {
        "comb_fb_0", "comb_fb_1", "comb_fb_2", "comb_fb_3", "comb_fb_4",
        "comb_fb_5", "comb_fb_6", "comb_fb_7", "comb_fb_8", "comb_fb_9"
    };
    constexpr const char* kCOMB_WET_MAX     = "comb_wet_max";

    // Elevation
    constexpr const char* kPINNA_NOTCH_HZ   = "pinna_notch_hz";
    constexpr const char* kPINNA_NOTCH_Q    = "pinna_notch_q";
    constexpr const char* kPINNA_SHELF_HZ   = "pinna_shelf_hz";
    constexpr const char* kPINNA_P1_FREQ_HZ = "pinna_p1_freq_hz";
    constexpr const char* kPINNA_P1_GAIN_DB = "pinna_p1_gain_db";
    constexpr const char* kPINNA_P1_Q       = "pinna_p1_q";
    constexpr const char* kPINNA_N2_OFFSET  = "pinna_n2_offset_hz";
    constexpr const char* kPINNA_N2_GAIN_DB = "pinna_n2_gain_db";
    constexpr const char* kPINNA_N2_Q       = "pinna_n2_q";
    constexpr const char* kPINNA_N1_MIN_HZ  = "pinna_n1_min_hz";
    constexpr const char* kPINNA_N1_MAX_HZ  = "pinna_n1_max_hz";
    constexpr const char* kCHEST_DELAY_MS   = "chest_delay_ms";
    constexpr const char* kCHEST_GAIN_DB    = "chest_gain_db";
    constexpr const char* kFLOOR_DELAY_MS   = "floor_delay_ms";
    constexpr const char* kFLOOR_GAIN_DB    = "floor_gain_db";
    constexpr const char* kFLOOR_ABS_HZ     = "floor_abs_hz";

    // Near-field
    constexpr const char* kNEAR_FIELD_LF_HZ     = "near_field_lf_hz";
    constexpr const char* kNEAR_FIELD_LF_MAX_DB = "near_field_lf_max_db";

    // Distance
    constexpr const char* kDIST_DELAY_MAX_MS = "dist_delay_max_ms";
    constexpr const char* kDIST_SMOOTH_MS    = "dist_smooth_ms";
    constexpr const char* kAIR_ABS_MAX_HZ    = "air_abs_max_hz";
    constexpr const char* kAIR_ABS_MIN_HZ    = "air_abs_min_hz";
    constexpr const char* kAIR_ABS_2_MAX_HZ  = "air_abs_2_max_hz";
    constexpr const char* kAIR_ABS_2_MIN_HZ  = "air_abs_2_min_hz";
    constexpr const char* kDIST_GAIN_FLOOR_DB = "dist_gain_floor_db";
    constexpr const char* kDIST_GAIN_MAX      = "dist_gain_max";

    // Head shadow
    constexpr const char* kHEAD_SHADOW_FULL_OPEN_HZ = "head_shadow_full_open_hz";

    // Presence shelf
    constexpr const char* kPRES_SHELF_FREQ   = "presence_shelf_freq_hz";
    constexpr const char* kPRES_SHELF_MAX_DB = "presence_shelf_max_db";

    // Ear canal
    constexpr const char* kEAR_CANAL_FREQ    = "ear_canal_freq_hz";
    constexpr const char* kEAR_CANAL_Q       = "ear_canal_q";
    constexpr const char* kEAR_CANAL_MAX_DB  = "ear_canal_max_db";

    // Aux send
    constexpr const char* kAUX_SEND_MAX_DB   = "aux_send_gain_max_db";

    // Geometry
    constexpr const char* kSPHERE_RADIUS     = "sphere_radius";
    constexpr const char* kVERT_MONO_CYL     = "vert_mono_cyl_radius";

    // Test tone
    constexpr const char* kTEST_TONE_ENABLED  = "test_tone_enabled";
    constexpr const char* kTEST_TONE_GAIN_DB  = "test_tone_gain_db";
    constexpr const char* kTEST_TONE_PITCH_HZ = "test_tone_pitch_hz";
    constexpr const char* kTEST_TONE_PULSE_HZ = "test_tone_pulse_hz";
    constexpr const char* kTEST_TONE_WAVEFORM = "test_tone_waveform";

    // Interpolation
    constexpr const char* kDELAY_INTERP_MODE  = "delay_interp_mode";

    // Expanded pinna EQ (P5)
    constexpr const char* kSHOULDER_PEAK_FREQ = "shoulder_peak_freq_hz";
    constexpr const char* kSHOULDER_PEAK_Q    = "shoulder_peak_q";
    constexpr const char* kSHOULDER_PEAK_MAX_DB = "shoulder_peak_max_db";
    constexpr const char* kCONCHA_NOTCH_FREQ  = "concha_notch_freq_hz";
    constexpr const char* kCONCHA_NOTCH_Q     = "concha_notch_q";
    constexpr const char* kCONCHA_NOTCH_MAX_DB = "concha_notch_max_db";
    constexpr const char* kUPPER_PINNA_FREQ   = "upper_pinna_freq_hz";
    constexpr const char* kUPPER_PINNA_Q      = "upper_pinna_q";
    constexpr const char* kUPPER_PINNA_MIN_DB = "upper_pinna_min_db";
    constexpr const char* kUPPER_PINNA_MAX_DB = "upper_pinna_max_db";
    constexpr const char* kTRAGUS_NOTCH_FREQ  = "tragus_notch_freq_hz";
    constexpr const char* kTRAGUS_NOTCH_Q     = "tragus_notch_q";
    constexpr const char* kTRAGUS_NOTCH_MAX_DB = "tragus_notch_max_db";
    constexpr const char* kBYPASS_EXPANDED_PINNA = "bypass_expanded_pinna";

    // Per-feature bypass toggles
    constexpr const char* kBYPASS_ITD         = "bypass_itd";
    constexpr const char* kBYPASS_HEAD_SHADOW = "bypass_head_shadow";
    constexpr const char* kBYPASS_ILD         = "bypass_ild";
    constexpr const char* kBYPASS_NEAR_FIELD  = "bypass_near_field";
    constexpr const char* kBYPASS_REAR_SHADOW = "bypass_rear_shadow";
    constexpr const char* kBYPASS_PINNA_EQ    = "bypass_pinna_eq";
    constexpr const char* kBYPASS_COMB        = "bypass_comb";
    constexpr const char* kBYPASS_CHEST       = "bypass_chest";
    constexpr const char* kBYPASS_FLOOR       = "bypass_floor";
    constexpr const char* kBYPASS_DIST_GAIN   = "bypass_dist_gain";
    constexpr const char* kBYPASS_DOPPLER     = "bypass_doppler";
    constexpr const char* kBYPASS_AIR_ABS     = "bypass_air_abs";
    constexpr const char* kBYPASS_REVERB      = "bypass_reverb";
}

const std::unordered_map<juce::String, juce::String>& DevPanelComponent::getDescriptions()
{
    static const std::unordered_map<juce::String, juce::String> descs = {
        // Test Tone
        { "test_tone_enabled",   "Master on/off for the built-in test tone generator. Useful for auditioning spatial cues without external audio." },
        { "test_tone_gain_db",   "Output level of the test tone in dB. Keep low to avoid clipping when combined with input audio." },
        { "test_tone_pitch_hz",  "Fundamental frequency of the test tone. Mid-range tones (300-1000 Hz) are best for hearing ITD/ILD cues clearly." },
        { "test_tone_pulse_hz",  "Rate at which the test tone pulses on/off. Pulsing helps distinguish direct vs. reflected sound in the spatial field." },
        { "test_tone_waveform",  "Waveform shape of the test tone (sine, saw, etc.). Harmonically rich waveforms reveal filter and comb effects more clearly." },

        // X-Axis: Left/Right
        { "itd_max_ms",          "Maximum interaural time difference in ms. Models the extra path length to the far ear (~0.7 ms for humans). Primary azimuth cue below ~1.5 kHz." },
        { "head_shadow_hz",      "Cutoff frequency of the head-shadow low-pass filter on the far ear. The head blocks high frequencies, creating an ILD that dominates azimuth perception above ~1.5 kHz." },
        { "ild_max_db",          "Maximum interaural level difference in dB. The far ear receives less energy due to head shadowing. Works with head_shadow_hz to encode left/right position." },
        { "vert_mono_cyl_radius","Radius of the vertical mono-blend cylinder. Sources within this radius blend toward mono to prevent extreme hard-panning at dead-center positions." },

        // Y-Axis: Front/Back
        { "rear_shadow_hz",      "Low-pass cutoff applied to rear sources. The pinna naturally attenuates high frequencies from behind. Lower values push the source further 'behind' the listener." },
        { "presence_shelf_freq_hz", "Centre frequency of the presence shelf filter. Boosting the 2-5 kHz range creates a forward/present quality; cutting it moves the source behind the head." },
        { "presence_shelf_max_db",  "Maximum gain/cut of the presence shelf in dB. Applied proportionally to front-back position (Y-axis). Positive = front boost, negative at rear." },
        { "ear_canal_freq_hz",   "Resonant frequency of the ear-canal model (peak filter). The real ear canal resonates around 2.5-3 kHz and amplifies front-facing sound. Scales with Y-axis." },
        { "ear_canal_q",         "Q (bandwidth) of the ear-canal resonance. Higher Q = narrower, more pronounced peak. Real ear canal Q is roughly 5-10." },
        { "ear_canal_max_db",    "Maximum boost of the ear-canal resonance in dB. Applied at full strength when the source is directly in front; fades to zero behind." },

        // Z-Axis: Above/Below
        { "pinna_notch_hz",      "Centre frequency of the pinna N1 notch filter. The pinna creates a narrow spectral notch whose frequency shifts with elevation — higher source = higher notch frequency." },
        { "pinna_notch_q",       "Q of the pinna N1 notch. Narrow notch (high Q) gives a sharper elevation cue. Real pinna notches are fairly narrow (Q ~ 5-15)." },
        { "pinna_shelf_hz",      "Cutoff of the pinna high-shelf filter. Elevated sources receive a subtle high-frequency boost from outer-ear reflections." },
        { "pinna_p1_freq_hz",    "Centre frequency of the P1 fixed pinna peak. A broadband peak around 5 kHz models outer-ear resonance. Does not shift with elevation." },
        { "pinna_p1_gain_db",    "Gain of the P1 pinna peak in dB. Positive = boost. Models the broadband gain of the outer-ear scapha resonance." },
        { "pinna_p1_q",          "Q of the P1 pinna peak. Lower Q = broader peak. Real pinna P1 is fairly broad (Q ~ 1-3)." },
        { "pinna_n2_offset_hz",  "Frequency offset of the N2 secondary notch from N1. N2 = N1 + offset. Models a second pinna interference notch." },
        { "pinna_n2_gain_db",    "Gain of the N2 secondary notch in dB. Typically negative (attenuation). Depth of the second pinna spectral notch." },
        { "pinna_n2_q",          "Q of the N2 secondary notch. Higher Q = narrower, sharper notch." },
        { "pinna_n1_min_hz",     "N1 frequency at Z=-1 (source below). The lowest the pinna notch sweeps. Together with N1 Max Hz defines the elevation-dependent frequency range." },
        { "pinna_n1_max_hz",     "N1 frequency at Z=+1 (source above). The highest the pinna notch sweeps. The notch interpolates linearly between min and max with elevation." },
        { "chest_delay_ms",      "Delay of the chest-bounce reflection in ms. Sound from below bounces off the torso, arriving ~0.4 ms late. Cues the brain that the source is low." },
        { "chest_gain_db",       "Gain of the chest-bounce reflection in dB. Typically negative (attenuated). Stronger reflection = stronger below cue." },
        { "floor_delay_ms",      "Delay of the floor reflection in ms. A second early reflection off the ground plane reinforces the below cue." },
        { "floor_gain_db",       "Gain of the floor reflection in dB. Typically well below 0 dB. Combined with chest bounce creates a convincing below sensation." },
        { "floor_abs_hz",        "HF absorption cutoff for the floor reflection in Hz. Models the fact that floors absorb high frequencies — lower value = more absorption." },
        { "near_field_lf_hz",    "Frequency of the near-field LF boost low-shelf filter. At close range, the ipsilateral ear gets a bass boost (proximity effect). Models real head-diffraction LF lift." },
        { "near_field_lf_max_db","Maximum near-field LF boost in dB at close range and full azimuth. Higher values give a stronger bass proximity effect." },
        { "comb_delay_",         "Delay time for one line of the pinna comb filter bank. Ten parallel delay lines model the complex interference pattern of the outer ear at different elevations." },
        { "comb_fb_",            "Feedback coefficient for one comb filter line. Controls how many times the signal recirculates. Higher values = more resonant, coloured sound." },
        { "comb_wet_max",        "Maximum wet mix of the pinna comb filter bank. Blends the comb-filtered signal with dry. Scales with elevation distance from the horizontal plane." },

        // Distance
        { "dist_delay_max_ms",   "Maximum propagation delay in ms at the far edge of the sphere. Models speed-of-sound travel time. This is a creative effect, not latency-compensated." },
        { "dist_smooth_ms",      "Smoothing time for distance parameter changes in ms. Prevents clicks when the source jumps in distance. Affects delay interpolation ramp." },
        { "air_abs_max_hz",      "Stage 1 low-pass cutoff at minimum distance (close, open/flat). At close range the filter is wide open. Stage 1 of 2." },
        { "air_abs_min_hz",      "Stage 1 low-pass cutoff at maximum distance (far, strong HF loss). Models high-frequency absorption by air — distant sounds lose treble." },
        { "air_abs_2_max_hz",    "Stage 2 low-pass cutoff at minimum distance (close, flat). Second cascade stage for steeper HF rolloff at distance. Stage 2 of 2." },
        { "air_abs_2_min_hz",    "Stage 2 low-pass cutoff at maximum distance (far). Combined with stage 1 gives ~12 dB/octave effective rolloff. Stage 2 of 2." },
        { "dist_gain_floor_db",  "Gain floor in dB at sphere boundary. Controls distance attenuation law steepness. -72 dB = strong inverse-square. Higher (less negative) = gentler rolloff." },
        { "dist_gain_max",       "Maximum distance gain multiplier (linear). Clamps close-range gain boost. 2.0 = up to +6 dB at very close range. 1.0 = no close-range boost." },
        { "head_shadow_full_open_hz", "Head shadow SVF cutoff when fully open (no shadowing). Caps the SVF frequency to prevent instability. Higher = wider open, but may cause SVF issues above 16 kHz at 44.1 kHz." },
        { "aux_send_gain_max_db","Maximum aux send level in dB at the far edge. Drives an external reverb bus to increase wet ratio with distance, simulating room reflections." },

        // Smoothing
        { "smooth_itd_ms",       "Smoothing time constant for ITD changes in ms. Prevents audible clicks when the source crosses the median plane. Too fast = clicks, too slow = laggy motion." },
        { "smooth_filter_ms",    "Smoothing time constant for filter coefficient changes in ms. Applies to head shadow, pinna, and ear-canal filters. Avoids zipper noise during motion." },
        { "smooth_gain_ms",      "Smoothing time constant for gain changes in ms. Covers ILD and distance attenuation. Fast enough for responsive motion, slow enough to avoid artefacts." },

        // Geometry
        { "sphere_radius",       "Radius of the spatial sphere in world units. All source positions are normalised to this radius. Larger values spread the spatial field wider." },

        // Interpolation
        { "delay_interp_mode",   "Delay line interpolation. 0 = Hermite (4-tap cubic). 1 = Sinc 2-tap. 2 = Sinc 4-tap. 3 = Sinc 8-tap. 4 = Sinc 16-tap. 5 = ZOH (nearest-neighbor, intentionally terrible). Applies to ALL delay lines." },

        // Per-feature bypass toggles
        { "bypass_itd",         "Bypass interaural time difference. Removes the delay offset between ears — the primary low-frequency azimuth cue." },
        { "bypass_head_shadow", "Bypass head shadow low-pass filter. Removes the far-ear HF attenuation that models the head blocking high frequencies." },
        { "bypass_ild",         "Bypass interaural level difference. Removes the far-ear gain reduction — the primary high-frequency azimuth cue." },
        { "bypass_near_field",  "Bypass near-field LF boost. Removes the proximity-effect bass lift on the near ear at close range." },
        { "bypass_rear_shadow", "Bypass rear shadow low-pass filter. Removes the HF rolloff applied to sources behind the listener." },
        { "bypass_pinna_eq",    "Bypass all pinna EQ (presence shelf, ear canal, P1 peak, N1/N2 notches, pinna shelf). Removes all elevation spectral cues." },
        { "bypass_comb",        "Bypass comb filter bank. Removes the pinna interference pattern that provides elevation detail." },
        { "bypass_chest",       "Bypass chest bounce reflection. Removes the early reflection cue for below-horizon sources." },
        { "bypass_floor",       "Bypass floor bounce reflection. Removes the ground-plane early reflection cue." },
        { "bypass_dist_gain",   "Bypass distance gain attenuation. Removes the inverse-square-law volume reduction with distance." },
        { "bypass_doppler",     "Bypass doppler / propagation delay. Removes the distance-dependent time delay and pitch shift." },
        { "bypass_air_abs",     "Bypass air absorption low-pass filters (both stages). Removes the HF rolloff that models atmospheric absorption at distance." },
        { "bypass_reverb",      "Bypass FDN reverb wet signal. Removes the reverb tail while keeping the FDN state fed for click-free re-enable." },

        // Expanded Pinna EQ (P5)
        { "shoulder_peak_freq_hz", "Centre frequency of the shoulder reflection peak. A low-frequency boost around 1.5 kHz from torso reflections, strongest when the source is below the horizontal plane." },
        { "shoulder_peak_q",       "Q (bandwidth) of the shoulder reflection peak. Lower Q = broader effect." },
        { "shoulder_peak_max_db",  "Maximum boost of the shoulder peak in dB at Z=-1 (source fully below). Fades to 0 dB at Z=+1 (above)." },
        { "concha_notch_freq_hz",  "Centre frequency of the concha notch. The concha (ear bowl) creates a resonant notch around 4 kHz, deepest when the source is below." },
        { "concha_notch_q",        "Q of the concha notch. Higher Q = narrower, more selective notch." },
        { "concha_notch_max_db",   "Maximum depth of the concha notch in dB at Z=-1 (below). 0 dB when source is above." },
        { "upper_pinna_freq_hz",   "Centre frequency of the upper pinna peak. The upper pinna ridge creates a peak around 12 kHz whose gain changes with elevation." },
        { "upper_pinna_q",         "Q of the upper pinna peak. Higher Q = narrower resonance." },
        { "upper_pinna_min_db",    "Gain of the upper pinna peak at Z=-1 (below). Typically negative (attenuation from below)." },
        { "upper_pinna_max_db",    "Gain of the upper pinna peak at Z=+1 (above). Typically positive (boost from above)." },
        { "tragus_notch_freq_hz",  "Centre frequency of the tragus notch. The tragus flap creates an 8.5 kHz notch active only when the source is behind AND below the listener." },
        { "tragus_notch_q",        "Q of the tragus notch. Higher Q = more selective. The tragus notch is typically narrow (Q ~ 3-4)." },
        { "tragus_notch_max_db",   "Maximum depth of the tragus notch in dB. Joint Y+Z mapping: only active when source is behind (Y<0) and below (Z<0)." },
        { "bypass_expanded_pinna", "Bypass all 4 expanded pinna bands (shoulder, concha, upper pinna, tragus). Core 6 pinna bands are unaffected by this toggle." },
        { "section:Expanded Pinna", "4 additional pinna EQ bands modelling concha, upper pinna, shoulder reflection, and tragus notch. Independent bypass from the core 6 pinna bands." },

        // Section descriptions
        { "section:Test Tone",        "Built-in signal generator for auditioning spatial DSP cues without needing external audio input." },
        { "section:X-Axis: Left/Right", "Azimuth (left/right) cues: interaural time difference (ITD), interaural level difference (ILD), and head shadow filtering." },
        { "section:Y-Axis: Front/Back", "Front/back disambiguation cues: rear shadow, presence shelf, and ear-canal resonance model." },
        { "section:Z-Axis: Above",      "Upward elevation cues: pinna notch (N1/N2), P1 peak, and high-shelf — models outer-ear spectral shaping for sources above the horizontal plane." },
        { "section:Z-Axis: Below",      "Downward elevation cues: chest-bounce and floor reflections that cue the brain to below-horizon sources." },
        { "section:Comb Filters",        "10-line pinna comb filter bank. Models complex interference patterns of the outer ear at different elevations." },
        { "section:Distance",          "Distance rendering: propagation delay, air absorption low-pass, gain attenuation, and aux reverb send." },
        { "section:Smoothing",         "Parameter smoothing time constants. Control how quickly DSP coefficients update during source motion to balance responsiveness vs. artefacts." },
        { "section:Interpolation",     "Delay line interpolation algorithm. Sweep from Hermite (4-tap) through Sinc 2/4/8/16 to compare CPU cost and audible artefacts. Mode 5 (ZOH) is intentionally terrible for diagnostics." },
        // Readout descriptions
        { "readout:ITD Samples",   "Current interaural time difference in samples. Positive = right ear delayed. Derived from azimuth angle and itd_max_ms." },
        { "readout:Shadow Cutoff", "Current head-shadow low-pass cutoff in Hz. Varies with azimuth — lower when source is far to one side." },
        { "readout:ILD Gain",      "Current interaural level difference as a linear gain. 1.0 = no difference; lower values mean more attenuation on the far ear." },
        { "readout:Rear Cutoff",   "Current rear-shadow low-pass cutoff in Hz. Drops as the source moves behind the listener." },
        { "readout:Comb Wet",      "Current wet mix of the pinna comb bank. Increases as the source moves away from the horizontal plane." },
        { "readout:Mono Blend",    "Current mono-blend factor. 1.0 = full stereo; approaches 0.0 as the source enters the vertical mono cylinder." },
        { "readout:Sample Rate",   "Audio sample rate in Hz reported by the host DAW. All delay/filter calculations depend on this value." },
        { "readout:Dist Delay",    "Current distance delay in samples. Proportional to normalised distance and dist_delay_max_ms." },
        { "readout:Dist Gain",     "Current distance attenuation as linear gain. Inverse-distance law: gain = 1 / (1 + distance)." },
        { "readout:Air Cutoff",    "Current air-absorption low-pass cutoff in Hz. Interpolated between air_abs_min_hz (close) and air_abs_max_hz (far)." },
        { "readout:Mod X",         "Current modulated X position after LFO. Shows the final horizontal position used for DSP, including any LFO displacement." },
    };
    return descs;
}

DevPanelComponent::DevPanelComponent(juce::AudioProcessorValueTreeState& apvts,
                                     xyzpan::DSPStateBridge* dspBridge)
    : dspBridge_(dspBridge)
{
    // Viewport owns the content component (false = don't delete on reassign)
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);  // vertical scroll only
    addAndMakeVisible(viewport_);

    // Drag handle sits on top of viewport (added after = higher Z-order)
    addAndMakeVisible(dragHandle_);

    // -------------------------------------------------------------------
    // Section 1: Test Tone (dev utility — stays at top)
    // -------------------------------------------------------------------
    beginSection("Test Tone");
    addDevToggle(kTEST_TONE_ENABLED,  apvts);
    addDevSlider(kTEST_TONE_GAIN_DB,  apvts);
    addDevSlider(kTEST_TONE_PITCH_HZ, apvts);
    addDevSlider(kTEST_TONE_PULSE_HZ, apvts);
    addDevSlider(kTEST_TONE_WAVEFORM, apvts);

    // -------------------------------------------------------------------
    // Section 2: X-Axis — Left/Right (binaural azimuth cues)
    // -------------------------------------------------------------------
    beginSection("X-Axis: Left/Right");
    addDevToggle(kBYPASS_ITD,         apvts);
    addDevToggle(kBYPASS_HEAD_SHADOW, apvts);
    addDevToggle(kBYPASS_ILD,         apvts);
    addDevToggle(kBYPASS_NEAR_FIELD,  apvts);
    addDevSlider(kITD_MAX_MS,               apvts);
    addDevSlider(kHEAD_SHADOW_HZ,           apvts);
    addDevSlider(kHEAD_SHADOW_FULL_OPEN_HZ, apvts);
    addDevSlider(kILD_MAX_DB,               apvts);
    addDevSlider(kNEAR_FIELD_LF_HZ,         apvts);
    addDevSlider(kNEAR_FIELD_LF_MAX_DB,     apvts);
    addDevSlider(kVERT_MONO_CYL,            apvts);
    if (dspBridge_ != nullptr) {
        addReadonlyLabel("ITD Samples");
        addReadonlyLabel("Shadow Cutoff");
        addReadonlyLabel("ILD Gain");
        addReadonlyLabel("Mono Blend");
    }

    // -------------------------------------------------------------------
    // Section 3: Y-Axis — Front/Back (front-rear spectral cues)
    // -------------------------------------------------------------------
    beginSection("Y-Axis: Front/Back");
    addDevToggle(kBYPASS_REAR_SHADOW, apvts);
    addDevSlider(kREAR_SHADOW_HZ,    apvts);
    addDevSlider(kPRES_SHELF_FREQ,   apvts);
    addDevSlider(kPRES_SHELF_MAX_DB, apvts);
    addDevSlider(kEAR_CANAL_FREQ,    apvts);
    addDevSlider(kEAR_CANAL_Q,       apvts);
    addDevSlider(kEAR_CANAL_MAX_DB,  apvts);
    if (dspBridge_ != nullptr) {
        addReadonlyLabel("Rear Cutoff");
    }

    // -------------------------------------------------------------------
    // Section 4: Z-Axis — Above (pinna spectral shaping)
    // -------------------------------------------------------------------
    beginSection("Z-Axis: Above");
    addDevToggle(kBYPASS_PINNA_EQ,   apvts);
    addDevSlider(kPINNA_NOTCH_HZ,    apvts);
    addDevSlider(kPINNA_NOTCH_Q,     apvts);
    addDevSlider(kPINNA_N1_MIN_HZ,   apvts);
    addDevSlider(kPINNA_N1_MAX_HZ,   apvts);
    addDevSlider(kPINNA_P1_FREQ_HZ,  apvts);
    addDevSlider(kPINNA_P1_GAIN_DB,  apvts);
    addDevSlider(kPINNA_P1_Q,        apvts);
    addDevSlider(kPINNA_N2_OFFSET,   apvts);
    addDevSlider(kPINNA_N2_GAIN_DB,  apvts);
    addDevSlider(kPINNA_N2_Q,        apvts);
    addDevSlider(kPINNA_SHELF_HZ,    apvts);

    // -------------------------------------------------------------------
    // Section 4b: Expanded Pinna (P5 — 4 additional bands)
    // -------------------------------------------------------------------
    beginSection("Expanded Pinna");
    addDevToggle(kBYPASS_EXPANDED_PINNA, apvts);
    addDevSlider(kSHOULDER_PEAK_FREQ,  apvts);
    addDevSlider(kSHOULDER_PEAK_Q,     apvts);
    addDevSlider(kSHOULDER_PEAK_MAX_DB, apvts);
    addDevSlider(kCONCHA_NOTCH_FREQ,   apvts);
    addDevSlider(kCONCHA_NOTCH_Q,      apvts);
    addDevSlider(kCONCHA_NOTCH_MAX_DB, apvts);
    addDevSlider(kUPPER_PINNA_FREQ,    apvts);
    addDevSlider(kUPPER_PINNA_Q,       apvts);
    addDevSlider(kUPPER_PINNA_MIN_DB,  apvts);
    addDevSlider(kUPPER_PINNA_MAX_DB,  apvts);
    addDevSlider(kTRAGUS_NOTCH_FREQ,   apvts);
    addDevSlider(kTRAGUS_NOTCH_Q,      apvts);
    addDevSlider(kTRAGUS_NOTCH_MAX_DB, apvts);

    // -------------------------------------------------------------------
    // Section 5: Z-Axis — Below (chest/floor reflections)
    // -------------------------------------------------------------------
    beginSection("Z-Axis: Below");
    addDevToggle(kBYPASS_CHEST,      apvts);
    addDevToggle(kBYPASS_FLOOR,      apvts);
    addDevSlider(kCHEST_DELAY_MS,    apvts);
    addDevSlider(kCHEST_GAIN_DB,     apvts);
    addDevSlider(kFLOOR_DELAY_MS,    apvts);
    addDevSlider(kFLOOR_GAIN_DB,     apvts);
    addDevSlider(kFLOOR_ABS_HZ,      apvts);

    // -------------------------------------------------------------------
    // Section 6: Comb Filters (pinna comb bank)
    // -------------------------------------------------------------------
    beginSection("Comb Filters");
    addDevToggle(kBYPASS_COMB,       apvts);
    for (int i = 0; i < 10; ++i)
        addDevSlider(kCOMB_DELAY[i], apvts);
    for (int i = 0; i < 10; ++i)
        addDevSlider(kCOMB_FB[i], apvts);
    addDevSlider(kCOMB_WET_MAX, apvts);
    if (dspBridge_ != nullptr) {
        addReadonlyLabel("Comb Wet");
    }

    // -------------------------------------------------------------------
    // Section 7: Distance (propagation cues)
    // -------------------------------------------------------------------
    beginSection("Distance");
    addDevToggle(kBYPASS_DIST_GAIN,   apvts);
    addDevToggle(kBYPASS_DOPPLER,     apvts);
    addDevToggle(kBYPASS_AIR_ABS,     apvts);
    addDevToggle(kBYPASS_REVERB,      apvts);
    addDevSlider(kDIST_DELAY_MAX_MS,  apvts);
    addDevSlider(kDIST_SMOOTH_MS,     apvts);
    addDevSlider(kDIST_GAIN_FLOOR_DB, apvts);
    addDevSlider(kDIST_GAIN_MAX,      apvts);
    addDevSlider(kAIR_ABS_MAX_HZ,     apvts);
    addDevSlider(kAIR_ABS_MIN_HZ,     apvts);
    addDevSlider(kAIR_ABS_2_MAX_HZ,   apvts);
    addDevSlider(kAIR_ABS_2_MIN_HZ,   apvts);
    addDevSlider(kAUX_SEND_MAX_DB,    apvts);
    if (dspBridge_ != nullptr) {
        addReadonlyLabel("Dist Delay");
        addReadonlyLabel("Dist Gain");
        addReadonlyLabel("Air Cutoff");
    }

    // -------------------------------------------------------------------
    // Section 8: Interpolation (delay line algorithm switch)
    // -------------------------------------------------------------------
    beginSection("Interpolation");
    addDevSlider(kDELAY_INTERP_MODE, apvts);

    // -------------------------------------------------------------------
    // Section 9: Smoothing (cross-axis utility)
    // -------------------------------------------------------------------
    beginSection("Smoothing");
    addDevSlider(kSMOOTH_ITD_MS,    apvts);
    addDevSlider(kSMOOTH_FILTER_MS, apvts);
    addDevSlider(kSMOOTH_GAIN_MS,   apvts);

    // -------------------------------------------------------------------
    // Status (non-collapsible, always visible at bottom)
    // -------------------------------------------------------------------
    if (dspBridge_ != nullptr) {
        auto* header = groupHeaders_.emplace_back(
            std::make_unique<juce::Label>()).get();
        header->setText("Status", juce::dontSendNotification);
        header->setFont(juce::Font(12.0f, juce::Font::bold));
        header->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        content_.addAndMakeVisible(header);

        // Not added to sections_ — non-collapsible
        currentSectionIdx_ = -1;  // readouts below go to statusReadoutChildren_

        addReadonlyLabel("Sample Rate");
        addReadonlyLabel("Mod X");

        startTimerHz(15);
    }

    relayout();
}

void DevPanelComponent::beginSection(const juce::String& title)
{
    auto* header = groupHeaders_.emplace_back(
        std::make_unique<juce::Label>()).get();

    header->setText(juce::String(juce::CharPointer_UTF8("\xe2\x96\xbe ")) + title,
                    juce::dontSendNotification);
    header->setFont(juce::Font(12.0f, juce::Font::bold));
    header->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    content_.addAndMakeVisible(header);

    // Make header clickable
    header->setInterceptsMouseClicks(true, false);
    header->addMouseListener(this, false);

    // Hover info: map section header to section key
    componentToDescKey_[header] = "section:" + title;

    sections_.push_back({ header, {}, false });
    currentSectionIdx_ = static_cast<int>(sections_.size()) - 1;
}

void DevPanelComponent::addDevSlider(const juce::String& paramID,
                                      juce::AudioProcessorValueTreeState& apvts)
{
    auto* param = apvts.getParameter(paramID);
    if (param == nullptr) return;  // safety: skip unknown params

    // Label
    auto* lbl = labels_.emplace_back(std::make_unique<juce::Label>()).get();
    lbl->setText(param->getName(50), juce::dontSendNotification);
    lbl->setFont(juce::Font(11.0f));
    lbl->setColour(juce::Label::textColourId, juce::Colours::silver);
    lbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(lbl);

    // Slider (linear horizontal)
    auto* slider = sliders_.emplace_back(std::make_unique<juce::Slider>()).get();
    slider->setSliderStyle(juce::Slider::LinearHorizontal);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);
    slider->setColour(juce::Slider::backgroundColourId, juce::Colours::darkgrey);
    content_.addAndMakeVisible(slider);

    // SliderAttachment — created and kept alive permanently
    attachments_.emplace_back(
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramID, *slider));

    // Hover info: register mouse listeners and map to paramID
    lbl->addMouseListener(this, false);
    slider->addMouseListener(this, true);
    componentToDescKey_[lbl] = paramID;
    componentToDescKey_[slider] = paramID;

    // Register with current section
    if (currentSectionIdx_ >= 0) {
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(lbl);
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(slider);
    }
}

void DevPanelComponent::addDevToggle(const juce::String& paramID,
                                      juce::AudioProcessorValueTreeState& apvts)
{
    auto* param = apvts.getParameter(paramID);
    if (param == nullptr) return;

    // Label
    auto* lbl = labels_.emplace_back(std::make_unique<juce::Label>()).get();
    lbl->setText(param->getName(50), juce::dontSendNotification);
    lbl->setFont(juce::Font(11.0f));
    lbl->setColour(juce::Label::textColourId, juce::Colours::silver);
    lbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(lbl);

    // ToggleButton
    auto* toggle = toggles_.emplace_back(std::make_unique<juce::ToggleButton>()).get();
    toggle->setButtonText({});
    content_.addAndMakeVisible(toggle);

    // ButtonAttachment
    toggleAtts_.emplace_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, paramID, *toggle));

    // Hover info: register mouse listeners and map to paramID
    lbl->addMouseListener(this, false);
    toggle->addMouseListener(this, true);
    componentToDescKey_[lbl] = paramID;
    componentToDescKey_[toggle] = paramID;

    // Register with current section
    if (currentSectionIdx_ >= 0) {
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(lbl);
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(toggle);
    }
}

void DevPanelComponent::addReadonlyLabel(const juce::String& name)
{
    // Name label
    auto* nameLbl = readoutNameLabels_.emplace_back(std::make_unique<juce::Label>()).get();
    nameLbl->setText(name, juce::dontSendNotification);
    nameLbl->setFont(juce::Font(11.0f));
    nameLbl->setColour(juce::Label::textColourId, juce::Colours::silver);
    nameLbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(nameLbl);

    // Value label
    auto* valLbl = readoutValueLabels_.emplace_back(std::make_unique<juce::Label>()).get();
    valLbl->setText("---", juce::dontSendNotification);
    valLbl->setFont(juce::Font(11.0f));
    valLbl->setColour(juce::Label::textColourId, juce::Colours::aqua);
    valLbl->setJustificationType(juce::Justification::centredLeft);
    content_.addAndMakeVisible(valLbl);

    // Name-based lookup for timerCallback
    readoutValueMap_[name] = valLbl;

    // Register with current section (or status if no section active)
    if (currentSectionIdx_ >= 0) {
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(nameLbl);
        sections_[static_cast<size_t>(currentSectionIdx_)].children.push_back(valLbl);
    } else {
        statusReadoutChildren_.push_back(nameLbl);
        statusReadoutChildren_.push_back(valLbl);
    }

    // Hover info: map both labels to readout key
    juce::String key = "readout:" + name;
    nameLbl->addMouseListener(this, false);
    valLbl->addMouseListener(this, false);
    componentToDescKey_[nameLbl] = key;
    componentToDescKey_[valLbl] = key;
}

void DevPanelComponent::relayout()
{
    int yPos = kPadding;
    const int contentW = kPadding * 2 + kLabelW + kSliderW + 8;

    // Layout collapsible sections
    for (auto& section : sections_) {
        // Position header
        section.header->setBounds(kPadding, yPos, kLabelW + kSliderW + 8, kGroupH);
        yPos += kGroupH;

        // Update header triangle prefix
        juce::String title = section.header->getText();
        // Strip existing triangle prefix
        if (title.startsWithChar(0x25B8) || title.startsWithChar(0x25BE))
            title = title.fromFirstOccurrenceOf(" ", false, false);
        section.header->setText(
            juce::String(section.collapsed
                ? juce::CharPointer_UTF8("\xe2\x96\xb8 ")   // ▸
                : juce::CharPointer_UTF8("\xe2\x96\xbe "))  // ▾
            + title, juce::dontSendNotification);

        if (section.collapsed) {
            // Hide all children
            for (auto* child : section.children)
                child->setVisible(false);
        } else {
            // Show and position children in pairs (label + control)
            for (size_t i = 0; i + 1 < section.children.size(); i += 2) {
                auto* lbl = section.children[i];
                auto* ctrl = section.children[i + 1];

                lbl->setVisible(true);
                lbl->setBounds(kPadding, yPos, kLabelW, kRowH);

                ctrl->setVisible(true);
                // Check if it's a toggle (ToggleButton) or slider
                if (dynamic_cast<juce::ToggleButton*>(ctrl) != nullptr)
                    ctrl->setBounds(kPadding + kLabelW + 4, yPos + (kRowH - 20) / 2, 24, 20);
                else
                    ctrl->setBounds(kPadding + kLabelW + 4, yPos + (kRowH - 24) / 2, kSliderW, 24);

                yPos += kRowH;
            }
        }

        yPos += kPadding;  // gap between sections
    }

    // Layout Status readouts (non-collapsible, always last)
    if (dspBridge_ != nullptr && !groupHeaders_.empty()) {
        auto* statusHeader = groupHeaders_.back().get();
        // Only position if it's not a section header
        bool isStatusHeader = true;
        for (auto& section : sections_) {
            if (section.header == statusHeader) {
                isStatusHeader = false;
                break;
            }
        }
        if (isStatusHeader) {
            statusHeader->setBounds(kPadding, yPos, kLabelW + kSliderW + 8, kGroupH);
            yPos += kGroupH;

            for (size_t i = 0; i + 1 < statusReadoutChildren_.size(); i += 2) {
                statusReadoutChildren_[i]->setBounds(kPadding, yPos, kLabelW, kRowH);
                statusReadoutChildren_[i + 1]->setBounds(kPadding + kLabelW + 4, yPos, kSliderW, kRowH);
                yPos += kRowH;
            }
        }
    }

    yPos += kPadding;
    content_.setSize(contentW, yPos);
}

void DevPanelComponent::timerCallback()
{
    if (dspBridge_ == nullptr)
        return;

    auto s = dspBridge_->read();
    char buf[32];

    auto set = [&](const char* name, const char* format, float val) {
        auto it = readoutValueMap_.find(name);
        if (it != readoutValueMap_.end()) {
            std::snprintf(buf, sizeof(buf), format, static_cast<double>(val));
            it->second->setText(buf, juce::dontSendNotification);
        }
    };

    set("ITD Samples",   "%.2f",    s.itdSamples);
    set("Shadow Cutoff", "%.0f Hz", s.shadowCutoffHz);
    set("ILD Gain",      "%.4f",    s.ildGainLinear);
    set("Mono Blend",    "%.3f",    s.monoBlend);
    set("Rear Cutoff",   "%.0f Hz", s.rearCutoffHz);
    set("Comb Wet",      "%.3f",    s.combWet);
    set("Dist Delay",    "%.1f",    s.distDelaySamp);
    set("Dist Gain",     "%.4f",    s.distGainLinear);
    set("Air Cutoff",    "%.0f Hz", s.airCutoffHz);
    set("Sample Rate",   "%.0f",    s.sampleRate);
    set("Mod X",         "%.4f",    s.modX);
}

void DevPanelComponent::mouseEnter(const juce::MouseEvent& event)
{
    // Walk up the parent chain from eventComponent until we find a mapped component
    for (auto* comp = event.eventComponent; comp != nullptr; comp = comp->getParentComponent()) {
        auto it = componentToDescKey_.find(comp);
        if (it != componentToDescKey_.end()) {
            const auto& key = it->second;
            const auto& descs = getDescriptions();

            // Exact lookup first
            auto descIt = descs.find(key);
            if (descIt != descs.end()) {
                currentInfoText_ = descIt->second;
            } else {
                // Prefix match for comb_delay_N / comb_fb_N
                currentInfoText_ = {};
                for (const auto& [prefix, text] : descs) {
                    if (key.startsWith(prefix)) {
                        currentInfoText_ = text;
                        break;
                    }
                }
            }

            repaint(0, getHeight() - kInfoBoxH, getWidth(), kInfoBoxH);
            return;
        }
    }
}

void DevPanelComponent::mouseExit(const juce::MouseEvent& event)
{
    // Walk up parent chain — only clear if we were showing something for this component
    for (auto* comp = event.eventComponent; comp != nullptr; comp = comp->getParentComponent()) {
        if (componentToDescKey_.find(comp) != componentToDescKey_.end()) {
            currentInfoText_ = {};
            repaint(0, getHeight() - kInfoBoxH, getWidth(), kInfoBoxH);
            return;
        }
    }
}

void DevPanelComponent::mouseDown(const juce::MouseEvent& event)
{
    // Collapsible section header click
    auto* source = event.eventComponent;
    for (auto& section : sections_) {
        if (section.header == source) {
            section.collapsed = !section.collapsed;
            relayout();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// DragHandle — thin overlay on left edge for resize
// ---------------------------------------------------------------------------
void DevPanelComponent::DragHandle::mouseDown(const juce::MouseEvent& e)
{
    owner.dragging_ = true;
    owner.dragStartX_ = e.getScreenX();
    owner.dragStartW_ = owner.getWidth();
}

void DevPanelComponent::DragHandle::mouseDrag(const juce::MouseEvent& e)
{
    if (!owner.dragging_) return;

    const int delta = owner.dragStartX_ - e.getScreenX();
    int newW = owner.dragStartW_ + delta;

    if (auto* parent = owner.getParentComponent()) {
        const int minW = 200;
        const int maxW = parent->getWidth() - 50;
        newW = juce::jlimit(minW, maxW, newW);
    }

    owner.customWidth_ = newW;
    if (auto* parent = owner.getParentComponent())
        owner.setBounds(parent->getWidth() - newW, 0, newW, parent->getHeight());
}

void DevPanelComponent::DragHandle::mouseUp(const juce::MouseEvent&)
{
    owner.dragging_ = false;
}

void DevPanelComponent::mouseWheelMove(const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& wheel)
{
    // Swallow wheel events so they scroll the viewport instead of
    // propagating to XYZPanGLView (which uses them for camera zoom).
    viewport_.mouseWheelMove(e, wheel);
}

void DevPanelComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(kInfoBoxH);  // reserve space for info box
    viewport_.setBounds(area);
    dragHandle_.setBounds(0, 0, kDragHandleW, getHeight());
    dragHandle_.toFront(false);  // keep on top of viewport
}

void DevPanelComponent::paint(juce::Graphics& g)
{
    // Dark functional background — intentionally plain (not alchemy theme)
    g.fillAll(juce::Colour(0xFF1E1E1E));

    // Left-edge drag handle — subtle grip indicator
    g.setColour(juce::Colours::grey.withAlpha(0.5f));
    g.fillRect(0, 0, 2, getHeight());
    // Draw grip dots (3 small lines centered vertically)
    const int midY = getHeight() / 2;
    g.setColour(juce::Colours::grey.withAlpha(0.7f));
    for (int i = -1; i <= 1; ++i)
        g.fillRect(1, midY + i * 8 - 1, 3, 2);

    // Thin right-edge border
    g.setColour(juce::Colours::grey.darker(0.3f));
    g.drawLine(static_cast<float>(getWidth() - 1), 0.0f,
               static_cast<float>(getWidth() - 1),
               static_cast<float>(getHeight()), 1.0f);

    // Info box at bottom
    auto infoArea = getLocalBounds().removeFromBottom(kInfoBoxH);
    g.setColour(juce::Colour(0xFF252525));
    g.fillRect(infoArea);

    // Top separator line
    g.setColour(juce::Colours::grey.withAlpha(0.4f));
    g.drawHorizontalLine(infoArea.getY(), static_cast<float>(infoArea.getX()),
                         static_cast<float>(infoArea.getRight()));

    auto textArea = infoArea.reduced(kPadding + 2, 4);
    if (currentInfoText_.isNotEmpty()) {
        g.setColour(juce::Colours::silver);
        g.setFont(juce::Font(10.5f));
        g.drawFittedText(currentInfoText_, textArea,
                         juce::Justification::topLeft, 4, 1.0f);
    } else {
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.setFont(juce::Font(10.5f));
        g.drawFittedText("Hover over a parameter for details", textArea,
                         juce::Justification::centred, 1, 1.0f);
    }
}
