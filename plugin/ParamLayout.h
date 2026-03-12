#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Factory function that creates the APVTS ParameterLayout for XYZPan.
// Defines X, Y, Z parameters with range [-1, 1], step 0.001.
// Y default = 1.0f (front position in Y-forward coordinate convention).
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
