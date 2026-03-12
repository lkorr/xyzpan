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

    return layout;
}
