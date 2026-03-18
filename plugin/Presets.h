#pragma once
#include <juce_core/juce_core.h>

// Factory preset definitions for XYZPan.
//
// Each preset XML must include ALL parameter IDs so that no parameter bleeds
// from a previously-loaded preset (APVTS replaceState only updates params
// present in the XML — Pitfall 4 from research).
//
// XML format: <XYZPanState> with one <PARAM id="..." value="..."/> per param.
// AudioParameterFloat  → value is the raw float
// AudioParameterChoice → value is the integer choice index (0-based)
// AudioParameterBool   → value is "0.0" or "1.0"
//
// Beat div default index = 6 (label "1", = quarter note, per kBeatDivDefaultIndex)
// kVerbDefaultWet = 0.0 (parameter default); preset Default uses 0.3 intentionally.

namespace XYZPresets {

struct Preset {
    const char* name;
    const char* xml;
};

// ---------------------------------------------------------------------------
// Preset 1: Default
// Source at front center (x=0, y=1, z=0), all LFOs off (depth=0),
// reverb wet=0.3, stereo width=0, all other params at engine defaults.
// This is the "init" preset.
// ---------------------------------------------------------------------------
static constexpr const char* kDefaultXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.0"/>
  <PARAM id="y" value="1.0"/>
  <PARAM id="z" value="0.0"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="300.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.5"/>
  <PARAM id="verb_decay" value="0.5"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.3"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="0.5"/>
  <PARAM id="lfo_x_depth" value="0.0"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="0.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.5"/>
  <PARAM id="lfo_y_depth" value="0.0"/>
  <PARAM id="lfo_y_phase" value="0.0"/>
  <PARAM id="lfo_y_waveform" value="0.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.5"/>
  <PARAM id="lfo_z_depth" value="0.0"/>
  <PARAM id="lfo_z_phase" value="0.0"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Preset 2: Orbit XY
// Source at center (x=0, y=0.5, z=0).
// LFO X: sine (0), rate=0.5Hz, depth=0.8, phase=0.
// LFO Y: sine (0), rate=0.5Hz, depth=0.8, phase=0.25 (quarter-phase offset
//         for circular XY orbit). LFO Z off (depth=0).
// Reverb wet=0.2.
// ---------------------------------------------------------------------------
static constexpr const char* kOrbitXYXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.0"/>
  <PARAM id="y" value="0.5"/>
  <PARAM id="z" value="0.0"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="300.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.5"/>
  <PARAM id="verb_decay" value="0.5"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.2"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="0.5"/>
  <PARAM id="lfo_x_depth" value="0.8"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="0.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.5"/>
  <PARAM id="lfo_y_depth" value="0.8"/>
  <PARAM id="lfo_y_phase" value="0.25"/>
  <PARAM id="lfo_y_waveform" value="0.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.5"/>
  <PARAM id="lfo_z_depth" value="0.0"/>
  <PARAM id="lfo_z_phase" value="0.0"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Preset 3: Slow Drift
// Source at (x=0.3, y=0.7, z=0.2). Triangle LFOs at slow rates for gentle
// ambient movement. Reverb wet=0.35.
// ---------------------------------------------------------------------------
static constexpr const char* kSlowDriftXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.3"/>
  <PARAM id="y" value="0.7"/>
  <PARAM id="z" value="0.2"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="300.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.5"/>
  <PARAM id="verb_decay" value="0.5"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.35"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="0.1"/>
  <PARAM id="lfo_x_depth" value="0.4"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="1.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.07"/>
  <PARAM id="lfo_y_depth" value="0.3"/>
  <PARAM id="lfo_y_phase" value="0.0"/>
  <PARAM id="lfo_y_waveform" value="1.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.05"/>
  <PARAM id="lfo_z_depth" value="0.2"/>
  <PARAM id="lfo_z_phase" value="0.0"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Preset 4: Behind You
// Source behind listener (x=0, y=-0.8, z=0). No LFOs.
// Reverb wet=0.4, size=0.7, decay=0.7 — long tail for rear placement.
// ---------------------------------------------------------------------------
static constexpr const char* kBehindYouXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.0"/>
  <PARAM id="y" value="-0.8"/>
  <PARAM id="z" value="0.0"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="300.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.7"/>
  <PARAM id="verb_decay" value="0.7"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.4"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="0.5"/>
  <PARAM id="lfo_x_depth" value="0.0"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="0.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.5"/>
  <PARAM id="lfo_y_depth" value="0.0"/>
  <PARAM id="lfo_y_phase" value="0.0"/>
  <PARAM id="lfo_y_waveform" value="0.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.5"/>
  <PARAM id="lfo_z_depth" value="0.0"/>
  <PARAM id="lfo_z_phase" value="0.0"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Preset 5: Fly Around
// Source near center (x=0, y=0.3, z=0). All 3 LFOs active at faster rates
// and high depth for complex 3D orbital motion. Reverb wet=0.25.
// ---------------------------------------------------------------------------
static constexpr const char* kFlyAroundXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.0"/>
  <PARAM id="y" value="0.3"/>
  <PARAM id="z" value="0.0"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="300.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.5"/>
  <PARAM id="verb_decay" value="0.5"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.25"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="1.2"/>
  <PARAM id="lfo_x_depth" value="0.9"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="0.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.8"/>
  <PARAM id="lfo_y_depth" value="0.9"/>
  <PARAM id="lfo_y_phase" value="0.33"/>
  <PARAM id="lfo_y_waveform" value="0.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.6"/>
  <PARAM id="lfo_z_depth" value="0.5"/>
  <PARAM id="lfo_z_phase" value="0.66"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Preset 6: Overhead
// Source above listener (x=0, y=0.5, z=0.9). No LFOs.
// Reverb wet=0.3, size=0.6. Demonstrates elevation cues.
// ---------------------------------------------------------------------------
static constexpr const char* kOverheadXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.0"/>
  <PARAM id="y" value="0.5"/>
  <PARAM id="z" value="0.9"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="300.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.6"/>
  <PARAM id="verb_decay" value="0.5"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.3"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="0.5"/>
  <PARAM id="lfo_x_depth" value="0.0"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="0.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.5"/>
  <PARAM id="lfo_y_depth" value="0.0"/>
  <PARAM id="lfo_y_phase" value="0.0"/>
  <PARAM id="lfo_y_waveform" value="0.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.5"/>
  <PARAM id="lfo_z_depth" value="0.0"/>
  <PARAM id="lfo_z_phase" value="0.0"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Preset 7: Near Whisper
// Source very close (x=0.1, y=0.2, z=0). No LFOs.
// Reverb wet=0.1 (dry intimate sound). dist_delay_max_ms=0 (no distance delay).
// Demonstrates proximity effect.
// ---------------------------------------------------------------------------
static constexpr const char* kNearWhisperXml = R"(
<XYZPanState>
  <PARAM id="x" value="0.1"/>
  <PARAM id="y" value="0.2"/>
  <PARAM id="z" value="0.0"/>
  <PARAM id="r" value="1.0"/>
  <PARAM id="itd_max_ms" value="0.72"/>
  <PARAM id="head_shadow_hz" value="1200.0"/>
  <PARAM id="ild_max_db" value="8.0"/>
  <PARAM id="rear_shadow_hz" value="4000.0"/>
  <PARAM id="smooth_itd_ms" value="8.0"/>
  <PARAM id="smooth_filter_ms" value="5.0"/>
  <PARAM id="smooth_gain_ms" value="5.0"/>
  <PARAM id="comb_delay_0" value="0.21"/>
  <PARAM id="comb_delay_1" value="0.37"/>
  <PARAM id="comb_delay_2" value="0.54"/>
  <PARAM id="comb_delay_3" value="0.68"/>
  <PARAM id="comb_delay_4" value="0.83"/>
  <PARAM id="comb_delay_5" value="0.97"/>
  <PARAM id="comb_delay_6" value="1.08"/>
  <PARAM id="comb_delay_7" value="1.23"/>
  <PARAM id="comb_delay_8" value="1.38"/>
  <PARAM id="comb_delay_9" value="1.5"/>
  <PARAM id="comb_fb_0" value="0.15"/>
  <PARAM id="comb_fb_1" value="0.14"/>
  <PARAM id="comb_fb_2" value="0.16"/>
  <PARAM id="comb_fb_3" value="0.13"/>
  <PARAM id="comb_fb_4" value="0.15"/>
  <PARAM id="comb_fb_5" value="0.14"/>
  <PARAM id="comb_fb_6" value="0.16"/>
  <PARAM id="comb_fb_7" value="0.13"/>
  <PARAM id="comb_fb_8" value="0.15"/>
  <PARAM id="comb_fb_9" value="0.14"/>
  <PARAM id="comb_wet_max" value="0.3"/>
  <PARAM id="pinna_notch_hz" value="8000.0"/>
  <PARAM id="pinna_notch_q" value="2.0"/>
  <PARAM id="pinna_shelf_hz" value="4000.0"/>
  <PARAM id="chest_delay_ms" value="2.0"/>
  <PARAM id="chest_gain_db" value="-8.0"/>
  <PARAM id="floor_delay_ms" value="20.0"/>
  <PARAM id="floor_gain_db" value="-5.0"/>
  <PARAM id="dist_delay_max_ms" value="0.0"/>
  <PARAM id="dist_smooth_ms" value="30.0"/>
  <PARAM id="air_abs_max_hz" value="22000.0"/>
  <PARAM id="air_abs_min_hz" value="8000.0"/>
  <PARAM id="verb_size" value="0.5"/>
  <PARAM id="verb_decay" value="0.5"/>
  <PARAM id="verb_damping" value="0.5"/>
  <PARAM id="verb_wet" value="0.1"/>
  <PARAM id="verb_pre_delay" value="50.0"/>
  <PARAM id="lfo_x_rate" value="0.5"/>
  <PARAM id="lfo_x_depth" value="0.0"/>
  <PARAM id="lfo_x_phase" value="0.0"/>
  <PARAM id="lfo_x_waveform" value="0.0"/>
  <PARAM id="lfo_x_smooth" value="0.0"/>
  <PARAM id="lfo_x_beat_div" value="6"/>
  <PARAM id="lfo_y_rate" value="0.5"/>
  <PARAM id="lfo_y_depth" value="0.0"/>
  <PARAM id="lfo_y_phase" value="0.0"/>
  <PARAM id="lfo_y_waveform" value="0.0"/>
  <PARAM id="lfo_y_smooth" value="0.0"/>
  <PARAM id="lfo_y_beat_div" value="6"/>
  <PARAM id="lfo_z_rate" value="0.5"/>
  <PARAM id="lfo_z_depth" value="0.0"/>
  <PARAM id="lfo_z_phase" value="0.0"/>
  <PARAM id="lfo_z_waveform" value="0.0"/>
  <PARAM id="lfo_z_smooth" value="0.0"/>
  <PARAM id="lfo_z_beat_div" value="6"/>
  <PARAM id="lfo_tempo_sync" value="0.0"/>
  <PARAM id="lfo_speed_mul" value="1.0"/>
  <PARAM id="stereo_width" value="0.0"/>
  <PARAM id="stereo_face_listener" value="0.0"/>
  <PARAM id="stereo_orbit_phase" value="0.0"/>
  <PARAM id="stereo_orbit_offset" value="0.0"/>
  <PARAM id="stereo_orbit_xy_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xy_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xy_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xy_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xy_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xy_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_xz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_xz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_xz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_xz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_xz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_waveform" value="0.0"/>
  <PARAM id="stereo_orbit_yz_rate" value="0.5"/>
  <PARAM id="stereo_orbit_yz_beat_div" value="6"/>
  <PARAM id="stereo_orbit_yz_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_reset_phase" value="0.0"/>
  <PARAM id="stereo_orbit_yz_depth" value="0.0"/>
  <PARAM id="stereo_orbit_yz_smooth" value="0.0"/>
  <PARAM id="stereo_orbit_tempo_sync" value="0.0"/>
  <PARAM id="stereo_orbit_speed_mul" value="1.0"/>
  <PARAM id="presence_shelf_freq_hz" value="3000.0"/>
  <PARAM id="presence_shelf_max_db" value="4.0"/>
  <PARAM id="ear_canal_freq_hz" value="2700.0"/>
  <PARAM id="ear_canal_q" value="2.0"/>
  <PARAM id="ear_canal_max_db" value="4.0"/>
  <PARAM id="aux_send_gain_max_db" value="6.0"/>
  <PARAM id="sphere_radius" value="1.732051"/>
  <PARAM id="vert_mono_cyl_radius" value="0.2"/>
  <PARAM id="test_tone_enabled" value="0.0"/>
  <PARAM id="test_tone_gain_db" value="-12.0"/>
  <PARAM id="test_tone_pitch_hz" value="100.0"/>
  <PARAM id="test_tone_pulse_hz" value="2.0"/>
  <PARAM id="test_tone_waveform" value="0.0"/>
</XYZPanState>
)";

// ---------------------------------------------------------------------------
// Factory preset array — accessed by getNumPrograms/getProgramName/setCurrentProgram
// ---------------------------------------------------------------------------
static const Preset kFactoryPresets[] = {
    { "Default",      kDefaultXml    },
    { "Orbit XY",     kOrbitXYXml    },
    { "Slow Drift",   kSlowDriftXml  },
    { "Behind You",   kBehindYouXml  },
    { "Fly Around",   kFlyAroundXml  },
    { "Overhead",     kOverheadXml   },
    { "Near Whisper", kNearWhisperXml },
};

static constexpr int kNumPresets = (int)(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));

} // namespace XYZPresets
