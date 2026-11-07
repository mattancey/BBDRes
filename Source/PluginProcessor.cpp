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
    loopDynP   = apvts.getRawParameterValue (ParamIDs::loopDyn);
    mixP       = apvts.getRawParameterValue (ParamIDs::mix);
    levelP     = apvts.getRawParameterValue (ParamIDs::outLevel);
    driveP     = apvts.getRawParameterValue (ParamIDs::drive);
    curveP     = apvts.getRawParameterValue (ParamIDs::driveCurve);
    bbdP       = apvts.getRawParameterValue (ParamIDs::bbd);
    noiseP     = apvts.getRawParameterValue (ParamIDs::noise);
    freezeP    = apvts.getRawParameterValue (ParamIDs::freeze);
    rootP      = apvts.getRawParameterValue (ParamIDs::root);
    scaleP     = apvts.getRawParameterValue (ParamIDs::scale);
    quantizeP  = apvts.getRawParameterValue (ParamIDs::quantize);
    lfoShapeP  = apvts.getRawParameterValue (ParamIDs::lfoShape);
    lfoSyncP   = apvts.getRawParameterValue (ParamIDs::lfoSync);
    lfoRateP   = apvts.getRawParameterValue (ParamIDs::lfoRate);
    lfoDepthP  = apvts.getRawParameterValue (ParamIDs::lfoDepth);
    lfoTargetP = apvts.getRawParameterValue (ParamIDs::lfoTarget);
    driftP     = apvts.getRawParameterValue (ParamIDs::drift);
    spreadP    = apvts.getRawParameterValue (ParamIDs::spread);
    crossP     = apvts.getRawParameterValue (ParamIDs::cross);
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
 
    juce::StringArray syncNames;
    for (const auto* nm : bbdres::kSyncNames)
        syncNames.add (nm);
 
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
 
    // --- Input ---
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::drive, 1 }, "Drive",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::driveCurve, 1 }, "Drive Curve",
        juce::StringArray { "Soft", "Asym", "Fold" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::inputLP, 1 }, "Input LP 12dB",
        Range (200.0f, 20000.0f, 0.0f, 0.3f), 8000.0f, Attr().withStringFromValueFunction (hz)));
 
    // --- Resonator (Loop Dyn est volontairement juste après Loop LP) ---
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
        juce::ParameterID { ParamIDs::loopDyn, 1 }, "Loop Dyn",
        Range (-1.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::bbd, 1 }, "BBD",
        Range (0.0f, 1.0f, 0.001f), 0.3f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::noise, 1 }, "Noise",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::freeze, 1 }, "Freeze", false));
 
    // --- Pitch ---
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::root, 1 }, "Root",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::scale, 1 }, "Scale", scaleNames, 1));
 
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::quantize, 1 }, "Quantize", false));
 
    // --- Mod ---
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::lfoShape, 1 }, "LFO Shape",
        juce::StringArray { "Sine", "Triangle", "Square", "Ramp", "Random", "Smooth Random" }, 0));
 
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::lfoSync, 1 }, "LFO Sync", syncNames, 0));
 
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
        juce::ParameterID { ParamIDs::drift, 1 }, "Drift",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    // --- Stereo ---
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::spread, 1 }, "Spread",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::cross, 1 }, "Cross",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));
 
    // --- Sortie ---
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
    t.noise     = noiseP->load();
    t.lfoRate   = lfoRateP->load();
    t.lfoDepth  = lfoDepthP->load();
    t.loopDyn   = loopDynP->load();
    t.drift     = driftP->load();
    t.spread    = spreadP->load();
    t.cross     = crossP->load();
    t.curve     = juce::roundToInt (curveP->load());
    t.root      = juce::jlimit (0, 11, juce::roundToInt (rootP->load()));
    t.scale     = juce::jlimit (0, bbdres::kNumScales - 1, juce::roundToInt (scaleP->load()));
    t.lfoShape  = juce::roundToInt (lfoShapeP->load());
    t.lfoTarget = juce::roundToInt (lfoTargetP->load());
    t.lfoSync   = juce::jlimit (0, bbdres::kNumSync - 1, juce::roundToInt (lfoSyncP->load()));
    t.quantize  = quantizeP->load() > 0.5f;
    t.freeze    = freezeP->load() > 0.5f;
    return t;
}
 
void BBDResAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
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
    noiseSm.prepare (controlRate, 0.05f);
    spreadSm.prepare (controlRate, 0.05f);
    crossSm.prepare (controlRate, 0.05f);
    freezeSm.prepare (controlRate, 0.05f);
 
    lfo.prepare (controlRate, 11u);
    // Dérives indépendantes à gauche et à droite (rythmes différents pour ne pas rester synchrones)
    driftTuneLfo[0].prepare (controlRate, 23u);   driftTuneLfo[0].setRateHz (0.37f);
    driftTuneLfo[1].prepare (controlRate, 29u);   driftTuneLfo[1].setRateHz (0.43f);
    driftFilterLfo[0].prepare (controlRate, 37u); driftFilterLfo[0].setRateHz (0.51f);
    driftFilterLfo[1].prepare (controlRate, 41u); driftFilterLfo[1].setRateHz (0.61f);
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
    noiseSm.snap (t.noise);
    spreadSm.snap (t.spread);
    crossSm.snap (t.cross);
    freezeSm.snap (t.freeze ? 1.0f : 0.0f);
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
 
    // --- Tempo et position de l'hôte (pour la synchronisation du LFO) ---
    double bpm = 120.0, ppqStart = 0.0;
    bool transportLocked = false;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm())
                if (*b > 1.0) bpm = *b;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition())
                {
                    ppqStart = *p;
                    transportLocked = true;
                }
        }
    }
    const float beatsPerCycle = bbdres::kSyncBeats[t.lfoSync];
 
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
 
        float lfoRaw;
        if (t.lfoSync == 0)
        {
            lfo.setRateHz (t.lfoRate);
            lfoRaw = lfo.next (t.lfoShape);
        }
        else if (transportLocked)
        {
            const double ppq = ppqStart + (double) start * bpm / (60.0 * currentSampleRate);
            lfoRaw = lfo.nextAt (t.lfoShape, bbdres::syncedPhase (ppq, beatsPerCycle));
        }
        else
        {
            lfo.setRateHz ((float) (bpm / 60.0 / (double) beatsPerCycle));
            lfoRaw = lfo.next (t.lfoShape);
        }
        const float lfoV = lfoRaw * t.lfoDepth;
 
        float lfoSemis = 0.0f, lfoFb = 0.0f, lfoOct = 0.0f;
        if (t.lfoTarget == 0)      lfoSemis = lfoV * 12.0f;   // +/- 12 demi-tons
        else if (t.lfoTarget == 1) lfoFb    = lfoV * 0.3f;    // +/- 30 % de feedback
        else                       lfoOct   = lfoV * 3.0f;    // +/- 3 octaves de filtre
 
        const float dynOct = t.loopDyn * envUnit * 3.0f;      // Loop Dyn : l'entrée pilote le filtre de boucle
        float driftSemis[2], driftOct[2];                      // dérive indépendante par canal
        for (int c = 0; c < 2; ++c)
        {
            driftSemis[c] = driftTuneLfo[c].next (5) * t.drift * 0.4f;    // jusqu'à +/- 40 cents
            driftOct[c]   = driftFilterLfo[c].next (5) * t.drift * 0.5f;  // jusqu'à +/- 0,5 octave
        }
 
        // --- Hauteur : LFO, puis quantification sur la gamme, puis accord fin ---
        float base = t.tune + lfoSemis;
        if (t.quantize)
            base = bbdres::quantizeToScale (base, t.root, bbdres::kScales[t.scale].mask);
        const float semisTarget = base + t.fine * 0.01f;
 
        const float fbTarget = juce::jlimit (0.0f, 1.0f, t.feedback + lfoFb);
        const float loopTarget = juce::jlimit (kMinLoopLog2, kMaxLoopLog2, t.loopLog2 + lfoOct + dynOct);
 
        // --- Lissage ---
        const float semisSm = tuneSm.next (juce::jlimit (18.0f, 114.0f, semisTarget));
        const float loopSmV = loopSm.next (loopTarget);
        const float spreadSemis = spreadSm.next (t.spread) * 0.5f;   // désaccord total : jusqu'à 50 cents
        const float spreadHalf = numCh > 1 ? spreadSemis * 0.5f : 0.0f;
 
        bbdres::ResonatorParams rp;
        rp.feedback  = fbSm.next (fbTarget);
        rp.inputLpHz = std::exp2 (inSm.next (t.inLog2));
        rp.drive     = driveSm.next (t.drive);
        rp.curve     = t.curve;
        rp.bbd       = bbdSm.next (t.bbd);
        rp.noise     = noiseSm.next (t.noise);
        rp.freeze    = freezeSm.next (t.freeze ? 1.0f : 0.0f);
        const float crossAmt = crossSm.next (t.cross);
        rp.cross     = numCh > 1 ? crossAmt : 0.0f;
        const float mixNew = mixSm.next (t.mix);
        const float lvlNew = levelSm.next (t.level);
 
        for (int c = 0; c < numCh; ++c)
        {
            bbdres::ResonatorParams rc = rp;
            rc.semitones = juce::jlimit (12.0f, 120.0f, semisSm + (c == 0 ? -spreadHalf : spreadHalf) + driftSemis[c]);
            rc.loopLpHz  = std::exp2 (juce::jlimit (kMinLoopLog2, kMaxLoopLog2, loopSmV + driftOct[c]));
            resonators[c].update (rc);
        }
 
        for (int i = 0; i < len; ++i)
        {
            const float k = (float) (i + 1) / (float) len;
            const float m = mixPrev + (mixNew - mixPrev) * k;
            const float l = levelPrev + (lvlNew - levelPrev) * k;
 
            // Sorties de boucle de l'échantillon précédent, pour le couplage entre canaux
            const float crossIn[2] = { resonators[1].loopOut(), resonators[0].loopOut() };
 
            for (int c = 0; c < numCh; ++c)
            {
                float& s = data[c][start + i];
                const float dry = s;
                const float wet = resonators[c].process (dry, crossIn[c]);
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
 
