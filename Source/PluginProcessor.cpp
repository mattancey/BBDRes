#include "PluginProcessor.h"
#include "Params.h"
#include "PluginEditor.h"

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
    holdLenP   = apvts.getRawParameterValue (ParamIDs::holdLen);
    holdQuantP = apvts.getRawParameterValue (ParamIDs::holdQuant);
    holdMixP   = apvts.getRawParameterValue (ParamIDs::holdMix);
    holdHpP    = apvts.getRawParameterValue (ParamIDs::holdHP);
    holdLpP    = apvts.getRawParameterValue (ParamIDs::holdLP);
    lowCutP    = apvts.getRawParameterValue (ParamIDs::lowCut);
    pultecP    = apvts.getRawParameterValue (ParamIDs::pultec);
    seqOnP     = apvts.getRawParameterValue (ParamIDs::seqOn);
    seqLengthP = apvts.getRawParameterValue (ParamIDs::seqLength);
    seqStepP   = apvts.getRawParameterValue (ParamIDs::seqStep);
    seqQuantP  = apvts.getRawParameterValue (ParamIDs::seqQuant);
    seqGlideP  = apvts.getRawParameterValue (ParamIDs::seqGlide);
    for (int i = 0; i < bbdres::kMaxSeqSteps; ++i)
    {
        stepP[i]   = apvts.getRawParameterValue (ParamIDs::kStepIDs[i]);
        stepOnP[i] = apvts.getRawParameterValue (ParamIDs::kStepOnIDs[i]);
    }
    lfoPhaseP  = apvts.getRawParameterValue (ParamIDs::lfoPhase);
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
    auto degrees = [] (float v, int) { return juce::String (juce::roundToInt (v * 360.0f)) + " deg"; };
    auto holdTime = [] (float v, int)
    {
        const float sec = bbdres::holdSecondsFromSlider (v);
        return sec < 1.0f ? juce::String (juce::roundToInt (sec * 1000.0f)) + " ms" : juce::String (sec, 2) + " s";
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
        Range (200.0f, 20000.0f, 0.0f, 0.3f), 12000.0f, Attr().withStringFromValueFunction (hz)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::loopDyn, 1 }, "Loop Dyn",
        Range (-1.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));

    // Atténuateur de graves de la boucle : fréquence et profondeur (SOFT = transparent ... HP = passe-haut 6 dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::lowCut, 1 }, "Low Cut",
        Range (20.0f, 600.0f, 0.0f, 0.37f), 80.0f, Attr().withStringFromValueFunction (hz)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pultec, 1 }, "Pultec",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction ([] (float v, int)
        {
            if (v <= 0.001f) return juce::String ("SOFT");
            if (v >= 0.999f) return juce::String ("HP-6dB");
            return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::bbd, 1 }, "BBD",
        Range (0.0f, 1.0f, 0.001f), 0.3f, Attr().withStringFromValueFunction (percent)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::noise, 1 }, "Noise",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::freeze, 1 }, "Freeze", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::holdLen, 1 }, "Hold Length",
        Range (0.0f, 1.0f, 0.001f), 0.55f, Attr().withStringFromValueFunction (holdTime)));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::holdQuant, 1 }, "Hold Quantize", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::holdMix, 1 }, "Hold Mix",
        Range (0.0f, 1.0f, 0.001f), 1.0f, Attr().withStringFromValueFunction (percent)));

    // Filtre du looper : deux filtres 12 dB (passe-haut à gauche, passe-bas à droite), uniquement sur la boucle
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::holdHP, 1 }, "Hold HP 12dB",
        Range (20.0f, 20000.0f, 0.0f, 0.2f), 20.0f, Attr().withStringFromValueFunction (hz)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::holdLP, 1 }, "Hold LP 12dB",
        Range (20.0f, 20000.0f, 0.0f, 0.2f), 20000.0f, Attr().withStringFromValueFunction (hz)));

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
        juce::ParameterID { ParamIDs::lfoPhase, 1 }, "LFO Sync Phase",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (degrees)));

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

    // --- Séquenceur de pitch ---
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::seqOn, 1 }, "Seq On", false));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::seqLength, 1 }, "Seq Length", 1, bbdres::kMaxSeqSteps, 8));

    juce::StringArray seqStepNames;
    for (const auto& d : bbdres::kSeqSteps)
        seqStepNames.add (d.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::seqStep, 1 }, "Seq Step", seqStepNames, 5));   // 1/4

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::seqQuant, 1 }, "Seq Quantize", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::seqGlide, 1 }, "Seq Glide",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (percent)));

    auto semitones = [] (float v, int)
    {
        const int n = juce::roundToInt (v);
        return (n > 0 ? juce::String ("+") : juce::String()) + juce::String (n) + " st";
    };
    for (int i = 0; i < bbdres::kMaxSeqSteps; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::kStepIDs[i], 1 }, "Step " + juce::String (i + 1),
            Range (-24.0f, 24.0f, 1.0f), 0.0f, Attr().withStringFromValueFunction (semitones)));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::kStepOnIDs[i], 1 }, "Step " + juce::String (i + 1) + " On", true));
    }

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
    t.lfoOffset = lfoPhaseP->load();          // décalage de phase du LFO en mode sync (mémorisé à part de Rate)
    t.holdLen   = holdLenP->load();
    t.holdMix   = holdMixP->load();
    t.lowCutLog2 = std::log2 (lowCutP->load());
    t.pultec     = pultecP->load();
    t.holdHpLog2 = std::log2 (holdHpP->load());
    t.holdLpLog2 = std::log2 (holdLpP->load());
    t.holdQuant = holdQuantP->load() > 0.5f;
    t.seqOn     = seqOnP->load() > 0.5f;
    t.seqQuant  = seqQuantP->load() > 0.5f;
    t.seqLength = juce::jlimit (1, bbdres::kMaxSeqSteps, juce::roundToInt (seqLengthP->load()));
    t.seqStep   = juce::jlimit (0, bbdres::kNumSeqSteps - 1, juce::roundToInt (seqStepP->load()));
    t.seqGlide  = seqGlideP->load();
    for (int i = 0; i < bbdres::kMaxSeqSteps; ++i)
    {
        t.stepVal[i] = stepP[i]->load();
        t.stepOn[i]  = stepOnP[i]->load() > 0.5f;
    }
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
    syncPhaseAcc = 0.0;
    seqPosAcc = 0.0;
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
    holdHpSm.prepare (controlRate, 0.02f);
    lowCutSm.prepare (controlRate, 0.02f);
    pultecSm.prepare (controlRate, 0.02f);
    holdLpSm.prepare (controlRate, 0.02f);

    lfo.prepare (controlRate, 11u);
    // Dérives indépendantes à gauche et à droite (rythmes différents pour ne pas rester synchrones)
    driftTuneLfo[0].prepare (controlRate, 23u);   driftTuneLfo[0].setRateHz (0.60f);
    driftTuneLfo[1].prepare (controlRate, 29u);   driftTuneLfo[1].setRateHz (0.77f);
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
    holdHpSm.snap (t.holdHpLog2);
    lowCutSm.snap (t.lowCutLog2);
    pultecSm.snap (t.pultec);
    holdLpSm.snap (t.holdLpLog2);
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
    double bpm = 120.0, ppqStart = 0.0, barBeats = 4.0;
    bool transportLocked = false;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm())
                if (*b > 1.0) bpm = *b;
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0)
                    barBeats = (double) ts->numerator * 4.0 / (double) ts->denominator;   // longueur d'une mesure, en temps
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition())
                {
                    ppqStart = *p;
                    transportLocked = true;
                }
        }
    }
    uiBpm.store ((float) bpm);
    const float beatsPerCycle = bbdres::kSyncBeats[t.lfoSync];

    // Longueur du looper : libre, ou calée sur la division rythmique la plus proche du tempo de l'hôte
    float holdSec = bbdres::holdSecondsFromSlider (t.holdLen);
    if (t.holdQuant)
        holdSec = bbdres::quantizeHoldSeconds (holdSec, (float) bpm);

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
            // Calé sur la position de l'hôte ; le curseur Rate décale la phase (0 à 1 cycle)
            const double ppq = ppqStart + (double) start * bpm / (60.0 * currentSampleRate);
            const float ph = (float) std::fmod ((double) bbdres::syncedPhase (ppq, beatsPerCycle) + (double) t.lfoOffset, 1.0);
            lfoRaw = lfo.nextAt (t.lfoShape, ph);
        }
        else
        {
            // Transport à l'arrêt : le LFO tourne au bon tempo, avec le même décalage
            syncPhaseAcc += bpm / 60.0 / (double) beatsPerCycle * (double) kControlBlock / currentSampleRate;
            syncPhaseAcc -= std::floor (syncPhaseAcc);
            const float ph = (float) std::fmod (syncPhaseAcc + (double) t.lfoOffset, 1.0);
            lfoRaw = lfo.nextAt (t.lfoShape, ph);
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
            driftSemis[c] = driftTuneLfo[c].next (5) * t.drift * 1.0f;    // jusqu'à +/- 100 cents
            driftOct[c]   = driftFilterLfo[c].next (5) * t.drift * 0.5f;  // jusqu'à +/- 0,5 octave
        }

        // --- Séquenceur de pitch : décalage de chaque pas par rapport à Tune, calé ou non sur la gamme générale ---
        float seqBase = t.tune;
        int seqIndex = -1;
        if (t.seqOn)
        {
            const double stepBeats = bbdres::seqStepBeats (t.seqStep, barBeats);
            double posSteps;
            if (transportLocked)
                posSteps = (ppqStart + (double) start * bpm / (60.0 * currentSampleRate)) / stepBeats;
            else
            {
                seqPosAcc += bpm / 60.0 / stepBeats * (double) kControlBlock / currentSampleRate;
                posSteps = seqPosAcc;
            }
            const auto sq = bbdres::sequencerState (posSteps, t.seqLength, t.stepVal, t.stepOn, t.seqGlide);
            float noteA = t.tune + sq.prev, noteB = t.tune + sq.cur;
            if (t.seqQuant)
            {
                const unsigned scaleMask = bbdres::kScales[t.scale].mask;
                noteA = bbdres::quantizeToScale (noteA, t.root, scaleMask);
                noteB = bbdres::quantizeToScale (noteB, t.root, scaleMask);
            }
            seqBase = noteA + (noteB - noteA) * sq.t;   // glissando continu entre deux notes
            seqIndex = sq.index;
        }
        uiSeqStep.store (seqIndex);

        // --- Hauteur : séquenceur et LFO, puis quantification générale, puis accord fin ---
        float base = seqBase + lfoSemis;
        if (t.quantize)
            base = bbdres::quantizeToScale (base, t.root, bbdres::kScales[t.scale].mask);
        const float semisTarget = base + t.fine * 0.01f;

        const float fbTarget = juce::jlimit (0.0f, 1.0f, t.feedback + lfoFb);
        const float loopTarget = juce::jlimit (kMinLoopLog2, kMaxLoopLog2, t.loopLog2 + lfoOct + dynOct);

        // --- Lissage ---
        const float semisSm = tuneSm.next (juce::jlimit (18.0f, 114.0f, semisTarget));
        uiPitch.store (semisSm);                       // pour le clavier de l'interface
        const float loopSmV = loopSm.next (loopTarget);
        const float spreadSemis = spreadSm.next (t.spread) * 1.0f;   // désaccord total : jusqu'à 100 cents (+/- 50 par canal)
        const float spreadHalf = numCh > 1 ? spreadSemis * 0.5f : 0.0f;

        bbdres::ResonatorParams rp;
        rp.feedback  = fbSm.next (fbTarget);
        rp.inputLpHz = std::exp2 (inSm.next (t.inLog2));
        rp.drive     = driveSm.next (t.drive);
        rp.curve     = t.curve;
        rp.bbd       = bbdSm.next (t.bbd);
        rp.holdHpHz  = std::exp2 (holdHpSm.next (t.holdHpLog2));
        rp.lowCutHz  = std::exp2 (lowCutSm.next (t.lowCutLog2));
        rp.lowCutAmt = pultecSm.next (t.pultec);
        rp.holdLpHz  = std::exp2 (holdLpSm.next (t.holdLpLog2));
        rp.noise     = noiseSm.next (t.noise);
        rp.freeze    = t.freeze ? 1.0f : 0.0f;   // front montant = fige le son
        const float crossAmt = crossSm.next (t.cross);
        rp.cross     = numCh > 1 ? crossAmt : 0.0f;
        const float mixNew = mixSm.next (t.mix);
        const float lvlNew = levelSm.next (t.level);

        for (int c = 0; c < numCh; ++c)
        {
            bbdres::ResonatorParams rc = rp;
            rc.holdSeconds    = holdSec;
            rc.holdMix        = t.holdMix;
            rc.holdSpeedSemis = driftSemis[c] * 0.25f;   // la mémoire dérive un peu, comme une bande qui vit
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
    return new BBDResAudioProcessorEditor (*this);
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
