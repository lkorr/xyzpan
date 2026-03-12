#include "PluginProcessor.h"
#include "ParamIDs.h"
#include "xyzpan/Types.h"

XYZPanProcessor::XYZPanProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",   juce::AudioChannelSet::mono(),   true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "XYZPanState", createParameterLayout()) {
    xParam = apvts.getRawParameterValue(ParamID::X);
    yParam = apvts.getRawParameterValue(ParamID::Y);
    zParam = apvts.getRawParameterValue(ParamID::Z);

    jassert(xParam != nullptr);
    jassert(yParam != nullptr);
    jassert(zParam != nullptr);
}

void XYZPanProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);
}

void XYZPanProcessor::releaseResources() {
    // Nothing to release in Phase 1
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
    params.x = xParam->load();
    params.y = yParam->load();
    params.z = zParam->load();

    // Build input channel pointer array
    const float* inputs[2] = {
        buffer.getNumChannels() > 0 ? buffer.getReadPointer(0) : nullptr,
        buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr
    };
    const int numIn = buffer.getNumChannels();

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
