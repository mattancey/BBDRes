#include "PluginProcessor.h"
 
BBDResAudioProcessor::BBDResAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    gainParam = apvts.getRawParameterValue ("testGain");
}
 
juce::AudioProcessorValueTreeState::ParameterLayout BBDResAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
 
    // Paramètre de test de l'étape 1 (sera remplacé par les vrais paramètres)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "testGain", 1 },
        "Test Gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
 
    return layout;
}
 
void BBDResAudioProcessor::prepareToPlay (double, int)
{
    lastGain = juce::Decibels::decibelsToGain (gainParam->load());
}
 
bool BBDResAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
 
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
 
    return in == out;
}
 
void BBDResAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
 
    const float newGain = juce::Decibels::decibelsToGain (gainParam->load());
    buffer.applyGainRamp (0, buffer.getNumSamples(), lastGain, newGain);
    lastGain = newGain;
}
 
juce::AudioProcessorEditor* BBDResAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
 
void BBDResAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}
 
void BBDResAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
 
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BBDResAudioProcessor();
}
 
