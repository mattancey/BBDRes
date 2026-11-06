#include "PluginProcessor.h"
#include "Params.h"
 
BBDResAudioProcessor::BBDResAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    tuneP      = apvts.getRawParameterValue (ParamIDs::tune);
    fineP      = apvts.getRawParameterValue (ParamIDs::fine);
    feedbackP  = apvts.getRawParameterValue (ParamIDs::feedback);
    inputLpP   = apvts.getRawParameterValue (ParamIDs::inputLP);
    loopLpP    = apvts.getRawParameterValue (ParamIDs::loopLP);
    mixP       = apvts.getRawParameterValue (ParamIDs::mix);
    levelP     = apvts.getRawParameterValue (ParamIDs::outLevel);
    driveP     = apvts.getRawParameterValue (ParamIDs::drive);
    curveP     = apvts.getRawParameterValue (ParamIDs::driveCurve);
    bbdP       = apvts.getRawParameterValue (ParamIDs::bbd);
    rootP      = apvts.getRawParameterValue (ParamIDs::root);
    scaleP     = apvts.getRawParameterValue (ParamIDs::scale);
    quantizeP  = apvts.getRawParameterValue (ParamIDs::quantize);
    lfoShapeP  = apvts.getRawParameterValue (ParamIDs::lfoShape);
    lfoRateP   = apvts.getRawParameterValue (ParamIDs::lfoRate);
    lfoDepthP  = apvts.getRawParameterValue (ParamIDs::lfoDepth);
    lfoTargetP = apvts.getRawParameterValue (ParamIDs::lfoTarget);
    dynamicsP  = apvts.getRawParameterValue (ParamIDs::dynamics);
    driftP     = apvts.getRawParameterValue (ParamIDs::drift);
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
    auto lfoHz = [] (float v, int)
    {
        return juce::String (v, v < 1.0f ? 2 : 1) + " Hz";
    };
    auto note = [] (float v, int)
    {
        return juce::MidiMessage::getMidiNoteName (juce::roundToInt (v), true, true, 3);
    };
    auto cents = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; };
 
    juce::StringArray scaleNames;
    for (const auto& sc : bbdres::kScales)
        scaleNames.add (sc.name);
 
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
 
    // Input
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::drive, 1 }, "Drive",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::driveCurve, 1 }, "Drive Curve",
        juce::StringArray { "Soft", "Asym", "Fold" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::inputLP, 1 }, "Input LP 12dB",
        Range (200.0f, 20000.0f, 0.0f, 0.3f), 8000.0f, Attr().withStringFromValueFunction (hz)));
 
    // Resonator
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
        juce::ParameterID { ParamIDs::loopLP, 1 }, "Loop LP 6dB",
        Range (200.0f, 20000.0f, 0.0f, 0.3f), 6000.0f, Attr().withStringFromValueFunction (hz)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::bbd, 1 }, "BBD",
        Range (0.0f, 1.0f, 0.001f), 0.3f, Attr().withStringFromValueFunction (percent)));
 
    // Pitch
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::root, 1 }, "Root",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::scale, 1 }, "Scale", scaleNames, 1));
 
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::quantize, 1 }, "Quantize", false));
 
    // Mod
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::lfoShape, 1 }, "LFO Shape",
        juce::StringArray { "Sine", "Triangle", "Square", "Ramp", "Random", "Smooth Random" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::lfoRate, 1 }, "LFO Rate",
        Range (0.02f, 20.0f, 0.0f, 0.35f), 0.5f, Attr().withStringFromValueFunction (lfoHz)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::lfoDepth, 1 }, "LFO Depth",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::lfoTarget, 1 }, "LFO Target",
        juce::StringArray { "Tune", "Feedback", "Filter" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::dynamics, 1 }, "Dynamics",
        Range (-1.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::drift, 1 }, "Drift",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    // Sortie
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
    t.tune      = tuneP->load();
    t.fine      = fineP->load();
    t.feedback  = feedbackP->load();
    t.inLog2    = std::log2 (inputLpP->load());
    t.loopLog2  = std::log2 (loopLpP->load());
    t.mix       = mixP->load();
    t.level     = juce::Decibels::decibelsToGain (levelP->load());
    t.drive     = driveP->load();
    t.bbd       = bbdP->load();
    t.lfoRate   = lfoRateP->load();
    t.lfoDepth  = lfoDepthP->load();
    t.dynamics  = dynamicsP->load();
    t.drift     = driftP->load();
    t.curve     = juce::roundToInt (curveP->load());
    t.root      = juce::jlimit (0, 11, juce::roundToInt (rootP->load()));
    t.scale     = juce::jlimit (0, bbdres::kNumScales - 1, juce::roundToInt (scaleP->load()));
    t.lfoShape  = juce::roundToInt (lfoShapeP->load());
    t.lfoTarget = juce::roundToInt (lfoTargetP->load());
    t.quantize  = quantizeP->load() > 0.5f;
    return t;
}
 
void BBDResAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const float fs = (float) sampleRate;
    const float controlRate = fs / (float) kControlBlock;
 
    for (int i = 0; i < 2; ++i)
        resonators[i].prepare (fs, (std::uint32_t) (i + 1));
 
    tuneSm.prepare (controlRate, 0.03f);   // glissando de Tune d'environ 30 ms
    fbSm.prepare (controlRate, 0.02f);
    inSm.prepare (controlRate, 0.02f);
    loopSm.prepare (controlRate, 0.02f);
    mixSm.prepare (controlRate, 0.02f);
    levelSm.prepare (controlRate, 0.02f);
    driveSm.prepare (controlRate, 0.02f);
    bbdSm.prepare (controlRate, 0.05f);
 
    lfo.prepare (controlRate, 11u);
    driftTuneLfo.prepare (controlRate, 23u);
    driftFilterLfo.prepare (controlRate, 37u);
    driftTuneLfo.setRateHz (0.23f);
    driftFilterLfo.setRateHz (0.37f);
    env.prepare (controlRate);
 
    const auto t = readTargets();
    tuneSm.snap (t.tune + t.fine * 0.01f);
    fbSm.snap (t.feedback);
    inSm.snap (t.inLog2);
    loopSm.snap (t.loopLog2);
    mixSm.snap (t.mix);
    levelSm.snap (t.level);
    driveSm.snap (t.drive);
    bbdSm.snap (t.bbd);
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
 
    constexpr float kMinLoopLog2 = 6.64385619f;   // log2 (100 Hz)
    constexpr float kMaxLoopLog2 = 14.2877124f;   // log2 (20000 Hz)
 
    for (int start = 0; start < numSamples; start += kControlBlock)
    {
        const int len = juce::jmin (kControlBlock, numSamples - start);
 
        // --- Sources de modulation (cadence de contrôle) ---
        float acc = 0.0f;                                   // niveau d'entrée, avant traitement
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < len; ++i)
                acc += std::abs (data[c][start + i]);
        const float envUnit = bbdres::levelToUnit (env.next (acc / (float) (len * numCh)));
 
        lfo.setRateHz (t.lfoRate);
        const float lfoV = lfo.next (t.lfoShape) * t.lfoDepth;
        float lfoSemis = 0.0f, lfoFb = 0.0f, lfoOct = 0.0f;
        if (t.lfoTarget == 0)      lfoSemis = lfoV * 12.0f;   // +/- 12 demi-tons
        else if (t.lfoTarget == 1) lfoFb    = lfoV * 0.3f;    // +/- 30 % de feedback
        else                       lfoOct   = lfoV * 3.0f;    // +/- 3 octaves de filtre
 
        const float dynOct     = t.dynamics * envUnit * 3.0f;                  // Dynamics -> filtre
        const float driftSemis = driftTuneLfo.next (5) * t.drift * 0.3f;       // +/- 30 cents
        const float driftOct   = driftFilterLfo.next (5) * t.drift * 0.5f;     // +/- 0,5 octave
 
        // --- Hauteur : LFO, puis quantification sur la gamme, puis accord fin et dérive ---
        float base = t.tune + lfoSemis;
        if (t.quantize)
            base = bbdres::quantizeToScale (base, t.root, bbdres::kScales[t.scale].mask);
        const float semisTarget = juce::jlimit (18.0f, 114.0f, base + t.fine * 0.01f + driftSemis);
 
        const float fbTarget = juce::jlimit (0.0f, 1.0f, t.feedback + lfoFb);
        const float loopTarget = juce::jlimit (kMinLoopLog2, kMaxLoopLog2, t.loopLog2 + lfoOct + dynOct + driftOct);
 
        // --- Lissage et mise à jour des résonateurs ---
        bbdres::ResonatorParams rp;
        rp.semitones = tuneSm.next (semisTarget);
        rp.feedback  = fbSm.next (fbTarget);
        rp.inputLpHz = std::exp2 (inSm.next (t.inLog2));
        rp.loopLpHz  = std::exp2 (loopSm.next (loopTarget));
        rp.drive     = driveSm.next (t.drive);
        rp.curve     = t.curve;
        rp.bbd       = bbdSm.next (t.bbd);
        const float mixNew = mixSm.next (t.mix);
        const float lvlNew = levelSm.next (t.level);
 
        for (int c = 0; c < numCh; ++c)
            resonators[c].update (rp);
 
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
 
