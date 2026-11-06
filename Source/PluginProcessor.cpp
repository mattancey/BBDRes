#include "PluginProcessor.h"
#include "Params.h"
 
BBDResAudioProcessor::BBDResAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    tuneP     = apvts.getRawParameterValue (ParamIDs::tune);
    fineP     = apvts.getRawParameterValue (ParamIDs::fine);
    feedbackP = apvts.getRawParameterValue (ParamIDs::feedback);
    inputLpP  = apvts.getRawParameterValue (ParamIDs::inputLP);
    loopLpP   = apvts.getRawParameterValue (ParamIDs::loopLP);
    mixP      = apvts.getRawParameterValue (ParamIDs::mix);
    levelP    = apvts.getRawParameterValue (ParamIDs::outLevel);
}
 
juce::AudioProcessorValueTreeState::ParameterLayout BBDResAudioProcessor::createLayout()
{
    using Attr = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;
 
    auto percent = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto hz = [] (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    };
    auto note = [] (float v, int)
    {
        return juce::MidiMessage::getMidiNoteName (juce::roundToInt (v), true, true, 3);
    };
    auto cents = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; };
 
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::tune, 1 }, "Tune",
        Range (24.0f, 96.0f, 1.0f), 48.0f, Attr().withStringFromValueFunction (note)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::fine, 1 }, "Fine",
        Range (-100.0f, 100.0f, 1.0f), 0.0f, Attr().withStringFromValueFunction (cents)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::feedback, 1 }, "Feedback",
        Range (0.0f, 1.0f, 0.001f), 0.7f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::inputLP, 1 }, "Input LP 12dB",
        Range (200.0f, 20000.0f, 0.0f, 0.3f), 8000.0f, Attr().withStringFromValueFunction (hz)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::loopLP, 1 }, "Loop LP 6dB",
        Range (200.0f, 20000.0f, 0.0f, 0.3f), 6000.0f, Attr().withStringFromValueFunction (hz)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
        Range (0.0f, 1.0f, 0.001f), 0.5f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::outLevel, 1 }, "Level",
        Range (-24.0f, 12.0f, 0.1f), 0.0f, Attr().withLabel ("dB")));
 
    return layout;
}
 
BBDResAudioProcessor::Targets BBDResAudioProcessor::readTargets() const
{
    Targets t;
    t.semis    = tuneP->load() + fineP->load() * 0.01f;
    t.feedback = feedbackP->load();
    t.inLog2   = std::log2 (inputLpP->load());
    t.loopLog2 = std::log2 (loopLpP->load());
    t.mix      = mixP->load();
    t.level    = juce::Decibels::decibelsToGain (levelP->load());
    return t;
}
 
void BBDResAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const float fs = (float) sampleRate;
    const float controlRate = fs / (float) kControlBlock;
 
    for (auto& r : resonators)
        r.prepare (fs);
 
    tuneSm.prepare (controlRate, 0.03f);   // glissando de Tune d'environ 30 ms
    fbSm.prepare (controlRate, 0.02f);
    inSm.prepare (controlRate, 0.02f);
    loopSm.prepare (controlRate, 0.02f);
    mixSm.prepare (controlRate, 0.02f);
    levelSm.prepare (controlRate, 0.02f);
 
    const auto t = readTargets();
    tuneSm.snap (t.semis);
    fbSm.snap (t.feedback);
    inSm.snap (t.inLog2);
    loopSm.snap (t.loopLog2);
    mixSm.snap (t.mix);
    levelSm.snap (t.level);
    mixPrev = t.mix;
    levelPrev = t.level;
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
 
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0)
        return;
 
    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    const auto t = readTargets();
 
    for (int start = 0; start < numSamples; start += kControlBlock)
    {
        const int len = juce::jmin (kControlBlock, numSamples - start);
 
        const float semis  = tuneSm.next (t.semis);
        const float fb     = fbSm.next (t.feedback);
        const float inHz   = std::exp2 (inSm.next (t.inLog2));
        const float loopHz = std::exp2 (loopSm.next (t.loopLog2));
        const float mixNew = mixSm.next (t.mix);
        const float lvlNew = levelSm.next (t.level);
 
        for (int c = 0; c < numCh; ++c)
            resonators[c].update (semis, fb, inHz, loopHz);
 
        for (int i = 0; i < len; ++i)
        {
            const float k = (float) (i + 1) / (float) len;
            const float m = mixPrev + (mixNew - mixPrev) * k;
            const float l = levelPrev + (lvlNew - levelPrev) * k;
 
            for (int c = 0; c < numCh; ++c)
            {
                float& s = data[c][start + i];
                const float dry = s;
                const float wet = resonators[c].process (dry);
                s = (dry + (wet - dry) * m) * l;
            }
        }
        mixPrev = mixNew;
        levelPrev = lvlNew;
    }
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
 
