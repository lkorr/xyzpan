#include "PluginProcessor.h"
#include "ParamIDs.h"
#include "xyzpan/Types.h"

XYZPanProcessor::XYZPanProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",   juce::AudioChannelSet::mono(),   true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "XYZPanState", createParameterLayout()) {
    // Spatial position (Phase 1)
    xParam = apvts.getRawParameterValue(ParamID::X);
    yParam = apvts.getRawParameterValue(ParamID::Y);
    zParam = apvts.getRawParameterValue(ParamID::Z);

    jassert(xParam != nullptr);
    jassert(yParam != nullptr);
    jassert(zParam != nullptr);

    // Dev panel: binaural panning tuning (Phase 2)
    itdMaxParam       = apvts.getRawParameterValue(ParamID::ITD_MAX_MS);
    headShadowHzParam = apvts.getRawParameterValue(ParamID::HEAD_SHADOW_HZ);
    ildMaxDbParam     = apvts.getRawParameterValue(ParamID::ILD_MAX_DB);
    rearShadowHzParam = apvts.getRawParameterValue(ParamID::REAR_SHADOW_HZ);
    smoothItdParam    = apvts.getRawParameterValue(ParamID::SMOOTH_ITD_MS);
    smoothFilterParam = apvts.getRawParameterValue(ParamID::SMOOTH_FILTER_MS);
    smoothGainParam   = apvts.getRawParameterValue(ParamID::SMOOTH_GAIN_MS);

    jassert(itdMaxParam       != nullptr);
    jassert(headShadowHzParam != nullptr);
    jassert(ildMaxDbParam     != nullptr);
    jassert(rearShadowHzParam != nullptr);
    jassert(smoothItdParam    != nullptr);
    jassert(smoothFilterParam != nullptr);
    jassert(smoothGainParam   != nullptr);
}

void XYZPanProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);
}

void XYZPanProcessor::releaseResources() {
    engine.reset();
}

bool XYZPanProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // XYZPan requires exactly mono input and stereo output.
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::mono())   return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void XYZPanProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    // Snapshot current parameter values from APVTS atomics (safe on audio thread)
    xyzpan::EngineParams params;
    // Spatial position
    params.x = xParam->load();
    params.y = yParam->load();
    params.z = zParam->load();
    // Dev panel: binaural panning tuning
    params.maxITD_ms       = itdMaxParam->load();
    params.headShadowMinHz = headShadowHzParam->load();
    params.ildMaxDb        = ildMaxDbParam->load();
    params.rearShadowMinHz = rearShadowHzParam->load();
    params.smoothMs_ITD    = smoothItdParam->load();
    params.smoothMs_Filter = smoothFilterParam->load();
    params.smoothMs_Gain   = smoothGainParam->load();

    // Build input channel pointer array.
    // Use getTotalNumInputChannels() for numIn — NOT buffer.getNumChannels(), which
    // returns total channel slots (inputs + outputs). For mono-in/stereo-out, the buffer
    // has 2 channels but only channel 0 is valid input; channel 1 is uninitialized output.
    const int numIn = getTotalNumInputChannels();
    const float* inputs[2] = {
        numIn > 0 ? buffer.getReadPointer(0) : nullptr,
        numIn > 1 ? buffer.getReadPointer(1) : nullptr
    };

    // Output channel pointers (buffer has stereo output per bus layout)
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    engine.setParams(params);
    engine.process(inputs, numIn, outL, outR, buffer.getNumSamples());
}

juce::AudioProcessorEditor* XYZPanProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

void XYZPanProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void XYZPanProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new XYZPanProcessor();
}
