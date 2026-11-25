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
    for (int v = 0; v < bbdres::kNumVoices; ++v)
    {
        voiceOnP[v]   = apvts.getRawParameterValue (ParamIDs::kVoiceOnIDs[v]);
        voiceIntP[v]  = apvts.getRawParameterValue (ParamIDs::kVoiceIntIDs[v]);
        voiceLvlP[v]  = apvts.getRawParameterValue (ParamIDs::kVoiceLvlIDs[v]);
        voicePanP[v]  = apvts.getRawParameterValue (ParamIDs::kVoicePanIDs[v]);
        voiceSoloP[v] = apvts.getRawParameterValue (ParamIDs::kVoiceSoloIDs[v]);
        uiPitchVoice[v].store (-1.0f);
    }
    holdPhaseP = apvts.getRawParameterValue (ParamIDs::holdPhase);
    bounceThread.work = [this] { processBounceJob(); };
    bounceThread.startThread();
    for (int i = 0; i < bbdres::kNumFreezeSlots; ++i)
        uiSlotState[i].store (0);
    for (int i = 0; i < kCqtRing; ++i)
        cqtRing[i].store (0.0f, std::memory_order_relaxed);
    apvts.state.setProperty ("layout", 2, nullptr);   // version de la liste des durées de pas (voir setStateInformation)
    presets = std::make_unique<bbdpresets::PresetManager> (apvts);
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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::holdPhase, 1 }, "Hold Phase",
        Range (0.0f, 1.0f, 0.001f), 0.0f, Attr().withStringFromValueFunction (degrees)));

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

    // --- Résonateurs : 4 voix en parallèle (allumée, intervalle par rapport à Tune, niveau, balance, solo) ---
    static const float defaultInterval[4] = { 0.0f, 7.0f, 12.0f, -12.0f };
    static const float defaultLevel[4] = { 0.0f, -3.0f, -4.0f, -4.0f };
    static const float defaultPan[4] = { 0.0f, -0.35f, 0.35f, 0.0f };
    auto interval = [] (float v, int)
    {
        const int n = juce::roundToInt (v);
        return (n > 0 ? juce::String ("+") : juce::String()) + juce::String (n) + " st";
    };
    auto panText = [] (float v, int)
    {
        if (std::abs (v) < 0.005f) return juce::String ("C");
        return juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + (v < 0.0f ? " L" : " R");
    };
    for (int v = 0; v < bbdres::kNumVoices; ++v)
    {
        const juce::String n = "R" + juce::String (v + 1);
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::kVoiceOnIDs[v], 1 }, n + " On", v == 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::kVoiceIntIDs[v], 1 }, n + " Interval",
            Range (-24.0f, 24.0f, 1.0f), defaultInterval[v], Attr().withStringFromValueFunction (interval)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::kVoiceLvlIDs[v], 1 }, n + " Level",
            Range (-36.0f, 6.0f, 0.1f), defaultLevel[v], Attr().withLabel ("dB")));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::kVoicePanIDs[v], 1 }, n + " Pan",
            Range (-1.0f, 1.0f, 0.01f), defaultPan[v], Attr().withStringFromValueFunction (panText)));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::kVoiceSoloIDs[v], 1 }, n + " Solo", false));
    }

    // --- Séquenceur de pitch ---
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::seqOn, 1 }, "Seq On", false));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::seqLength, 1 }, "Seq Length", 1, bbdres::kMaxSeqSteps, 8));

    juce::StringArray seqStepNames;
    for (const auto& d : bbdres::kSeqSteps)
        seqStepNames.add (d.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::seqStep, 1 }, "Seq Step", seqStepNames, 7));   // 1/4

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
        juce::ParameterID { ParamIDs::outLevel, 1 }, "Wet Level",
        Range (-24.0f, 12.0f, 0.1f), 0.0f, Attr().withLabel ("dB")));

    // Fondu entre presets généraux et entre A et B (réglage global : les presets ne le modifient pas)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::presetFade, 1 }, "Preset Fade",
        Range (0.0f, 5.0f, 0.05f), 0.5f, Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " s"; })));

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
    t.holdPhase = holdPhaseP->load();
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
    for (int v = 0; v < bbdres::kNumVoices; ++v)
    {
        t.voiceOn[v]   = voiceOnP[v]->load() > 0.5f;
        t.voiceSolo[v] = voiceSoloP[v]->load() > 0.5f;
        t.voiceInt[v]  = voiceIntP[v]->load();
        t.voiceLvl[v]  = voiceLvlP[v]->load();
        t.voicePan[v]  = voicePanP[v]->load();
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

float BBDResAudioProcessor::voiceTargetGain (const Targets& t, int v) const
{
    bool anySolo = false;
    for (int i = 0; i < bbdres::kNumVoices; ++i)
        anySolo = anySolo || t.voiceSolo[i];
    const bool audible = bbdres::voiceAudible (t.voiceOn[v], t.voiceSolo[v], anySolo);
    return audible ? juce::Decibels::decibelsToGain (t.voiceLvl[v], -60.0f) : 0.0f;
}

void BBDResAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    syncPhaseAcc = 0.0;
    seqPosAcc = 0.0;
    seqTrans = {};
    seqHashValid = false;
    const float fs = (float) sampleRate;
    const float controlRate = fs / (float) kControlBlock;

    for (int v = 0; v < bbdres::kNumVoices; ++v)
        for (int c = 0; c < 2; ++c)
            resonators[v][c].prepare (fs, (std::uint32_t) (v * 2 + c + 1));

    for (int v = 0; v < bbdres::kNumVoices; ++v)
    {
        pitchSm[v].prepare (controlRate, 0.03f);   // glissando de la hauteur d'environ 30 ms
        voiceGainSm[v].prepare (controlRate, 0.02f);
        voicePanSm[v].prepare (controlRate, 0.02f);
    }
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
    // Dérives indépendantes pour chaque voix et chaque canal (rythmes et graines différents : l'ensemble ne reste pas synchrone)
    for (int v = 0; v < bbdres::kNumVoices; ++v)
        for (int c = 0; c < 2; ++c)
        {
            const int n = v * 2 + c;
            driftTuneLfo[v][c].prepare (controlRate, (std::uint32_t) (101 + 13 * n));
            driftTuneLfo[v][c].setRateHz (0.60f + 0.17f * (float) c + 0.05f * (float) v);
            driftFilterLfo[v][c].prepare (controlRate, (std::uint32_t) (211 + 17 * n));
            driftFilterLfo[v][c].setRateHz (0.51f + 0.10f * (float) c + 0.04f * (float) v);
        }
    env.prepare (controlRate);
    noiseGate.prepare (controlRate);
    holdPhaseSm.prepare (controlRate, 0.03f);   // la phase du Freeze est lissée : les déplacements du potard restent réguliers
    freezeBank.prepare (fs);   // (ré)alloue les mémoires de boucles, en conservant leur contenu
    for (auto& d : driveStage) d.prepare ((float) fs);   // x2 en dessous de 80 kHz, sinon rien
    for (auto& pl : players) pl.prepare (fs);
    activePlayer = 0;

    const auto t = readTargets();
    for (int v = 0; v < bbdres::kNumVoices; ++v)
    {
        pitchSm[v].snap (t.tune + t.voiceInt[v] + t.fine * 0.01f);
        voiceGainSm[v].snap (voiceTargetGain (t, v));
        voicePanSm[v].snap (t.voicePan[v]);
        voiceWasActive[v] = false;
    }
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
    holdPhaseSm.snap (t.holdPhase);
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

    // Mémoires de boucles Freeze : commandes venues de l'interface
    //  0 enregistrer (mémoire vide) : la boucle gelée est exportée EXACTEMENT comme elle joue (même fenêtre, même point
    //    de boucle, même longueur), la phase et la longueur réglées ensuite n'y changent rien ;
    //  1 rappeler / arrêter ; 2 tout arrêter ; 3 effacer (la mémoire redevient vide).
    {
        const int cmd = slotCommand.exchange (-1, std::memory_order_acq_rel);
        const int type = cmd >= 0 ? cmd / 8 : -1, idx = cmd >= 0 ? cmd % 8 : -1;
        if (type == 2)
            for (auto& pl : players) pl.stop();
        else if (idx >= 0 && idx < bbdres::kNumFreezeSlots)
        {
            if (type == 0 && ! freezeBank.used (idx) && ! bounceJob.pending.load (std::memory_order_acquire))
            {
                int voicesFrozen = 0;
                const int c1 = numCh > 1 ? 1 : 0;
                for (int v = 0; v < bbdres::kNumVoices; ++v)
                {
                    auto& jv = bounceJob.v[v];
                    jv.used = false;
                    if (! voiceWasActive[v]) continue;
                    if (resonators[v][0].exportLoop (jv.ex[0]) && resonators[v][c1].exportLoop (jv.ex[1]))
                    {
                        const auto pg = bbdres::panGains (voicePanSm[v].y);
                        const float g = voiceGainSm[v].y;
                        jv.gl = numCh > 1 ? g * pg.l : g;
                        jv.gr = numCh > 1 ? g * pg.r : g;
                        jv.used = true;
                        ++voicesFrozen;
                    }
                }
                if (voicesFrozen > 0)
                {
                    bounceJob.slot = idx;
                    bounceJob.fs = (float) currentSampleRate;
                    bounceJob.pending.store (true, std::memory_order_release);   // le rendu se fait sur un fil à part
                }
            }
            else if (type == 1)
            {
                auto& cur = players[activePlayer];
                if (cur.isPlaying() && cur.slot() == idx)
                    cur.stop();                                    // un nouveau clic sur la boucle qui joue l'arrête
                else if (freezeBank.used (idx))
                {
                    const int other = 1 - activePlayer;            // fondu : l'ancien s'éteint, le nouveau entre
                    cur.stop();
                    players[other].start (freezeBank.leftData (idx), freezeBank.rightData (idx), freezeBank.total (idx),
                                          freezeBank.preroll (idx), idx, 0.0f);   // toujours depuis le point de boucle : ni phase ni longueur
                    activePlayer = other;
                }
            }
            else if (type == 3)
            {
                for (auto& pl : players)
                    if (pl.slot() == idx) pl.stopNow();
                freezeBank.clear (idx);
            }
        }
    }

    // Le contenu de la séquence a changé (mémoire ou preset rappelé, pas modifié, SEQ activé) : la hauteur glisse
    // de l'ancienne valeur vers la nouvelle (environ 10 % d'un pas) au lieu de sauter.
    {
        unsigned hash = (t.seqOn ? 1u : 0u) | (t.seqQuant ? 2u : 0u);
        hash = hash * 31u + (unsigned) t.seqLength;
        hash = hash * 31u + (unsigned) t.seqStep;
        for (int i = 0; i < bbdres::kMaxSeqSteps; ++i)
            hash = hash * 31u + (unsigned) (juce::roundToInt (t.stepVal[i]) + 64) + (t.stepOn[i] ? 131u : 0u);
        if (seqHashValid && hash != lastSeqHash)
        {
            const double stepSeconds = bbdres::seqStepBeats (t.seqStep, barBeats) * 60.0 / bpm;
            seqTrans.start (lastSeqBase, (float) juce::jlimit (0.02, 0.4, 0.1 * stepSeconds));
        }
        lastSeqHash = hash;
        seqHashValid = true;
    }

    unsigned cqtWrite = cqtPos.load (std::memory_order_relaxed);
    const bool cqtActive = cqtOn.load (std::memory_order_relaxed);

    for (int start = 0; start < numSamples; start += kControlBlock)
    {
        const int len = juce::jmin (kControlBlock, numSamples - start);

        // --- Sources de modulation (cadence de contrôle) ---
        float acc = 0.0f;                                   // niveau d'entrée, avant traitement
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < len; ++i)
                acc += std::abs (data[c][start + i]);
        const float envLevel = env.next (acc / (float) (len * numCh));
        const float envUnit = bbdres::levelToUnit (envLevel);
        const float noiseGateGain = noiseGate.next (envLevel);   // le souffle ne tourne que si l'entrée dépasse -40 dB

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
        seqBase = seqTrans.apply (seqBase, (float) kControlBlock / (float) currentSampleRate);
        lastSeqBase = seqBase;

        // --- Hauteur commune : séquenceur et LFO (chaque voix y ajoute son intervalle) ---
        const float baseRaw = seqBase + lfoSemis;
        const unsigned scaleMask = bbdres::kScales[t.scale].mask;

        const float fbTarget = juce::jlimit (0.0f, 1.0f, t.feedback + lfoFb);
        const float loopTarget = juce::jlimit (kMinLoopLog2, kMaxLoopLog2, t.loopLog2 + lfoOct + dynOct);

        // --- Lissage des réglages communs ---
        const float loopSmV = loopSm.next (loopTarget);
        const float spreadSemis = spreadSm.next (t.spread) * 1.0f;   // désaccord total : jusqu'à 100 cents (+/- 50 par canal)
        const float spreadHalf = numCh > 1 ? spreadSemis * 0.5f : 0.0f;

        bbdres::ResonatorParams rp;
        rp.feedback  = fbSm.next (fbTarget);
        rp.inputLpHz = std::exp2 (inSm.next (t.inLog2));
        rp.drive     = driveSm.next (t.drive);
        rp.curve     = t.curve;
        for (auto& d : driveStage) d.update (rp.drive, rp.curve);
        rp.bbd       = bbdSm.next (t.bbd);
        rp.holdHpHz  = std::exp2 (holdHpSm.next (t.holdHpLog2));
        rp.lowCutHz  = std::exp2 (lowCutSm.next (t.lowCutLog2));
        rp.lowCutAmt = pultecSm.next (t.pultec);
        rp.holdLpHz  = std::exp2 (holdLpSm.next (t.holdLpLog2));
        rp.noise     = noiseSm.next (t.noise);
        rp.noiseGate = noiseGateGain;
        rp.freeze    = t.freeze ? 1.0f : 0.0f;   // front montant = fige le son
        rp.holdPhase = holdPhaseSm.next (t.holdPhase);
        for (auto& pl : players) pl.setFilters (rp.holdHpHz, rp.holdLpHz);   // les filtres du Hold agissent sur les mémoires ; ni phase ni longueur
        const float crossAmt = crossSm.next (t.cross);
        rp.cross     = numCh > 1 ? crossAmt : 0.0f;
        const float mixNew = mixSm.next (t.mix);
        const float lvlNew = levelSm.next (t.level);

        // --- Voix : hauteur (intervalle, gamme), niveau, balance, solo. Une voix muette ne consomme rien. ---
        float voiceGainL[bbdres::kNumVoices], voiceGainR[bbdres::kNumVoices];
        bool voiceActive[bbdres::kNumVoices];
        for (int v = 0; v < bbdres::kNumVoices; ++v)
        {
            const float gainTarget = voiceTargetGain (t, v);
            const float g = voiceGainSm[v].next (gainTarget);
            const auto pan = bbdres::panGains (voicePanSm[v].next (t.voicePan[v]));
            voiceGainL[v] = g * (numCh > 1 ? pan.l : 1.0f);
            voiceGainR[v] = g * pan.r;
            voiceActive[v] = g > 1.0e-4f || gainTarget > 0.0f;

            const float target = juce::jlimit (18.0f, 114.0f, bbdres::voicePitch (baseRaw, t.voiceInt[v], t.fine * 0.01f, t.quantize, t.root, scaleMask));
            const float voiceSemis = pitchSm[v].next (target);
            uiPitchVoice[v].store (voiceActive[v] ? voiceSemis : -1.0f);   // pour le clavier de l'interface
            if (v == 0) uiPitch.store (voiceSemis);

            if (! voiceActive[v]) { voiceWasActive[v] = false; continue; }
            if (! voiceWasActive[v])   // la voix (re)démarre : on repart d'une ligne à retard vide
            {
                for (int c = 0; c < 2; ++c) resonators[v][c].reset();
                voiceWasActive[v] = true;
            }

            float driftSemis[2], driftOct[2];   // dérive propre à chaque voix et à chaque canal
            for (int c = 0; c < 2; ++c)
            {
                driftSemis[c] = driftTuneLfo[v][c].next (5) * t.drift * 1.0f;    // jusqu'à +/- 100 cents
                driftOct[c]   = driftFilterLfo[v][c].next (5) * t.drift * 0.5f;  // jusqu'à +/- 0,5 octave
            }
            for (int c = 0; c < numCh; ++c)
            {
                bbdres::ResonatorParams rc = rp;
                rc.holdSeconds    = holdSec;
                rc.holdMix        = t.holdMix;
                rc.holdSpeedSemis = driftSemis[c] * 0.25f;   // la mémoire dérive un peu, comme une bande qui vit
                rc.semitones = juce::jlimit (12.0f, 120.0f, voiceSemis + (c == 0 ? -spreadHalf : spreadHalf) + driftSemis[c]);
                rc.loopLpHz  = std::exp2 (juce::jlimit (kMinLoopLog2, kMaxLoopLog2, loopSmV + driftOct[c]));
                resonators[v][c].update (rc);
            }
        }

        for (int i = 0; i < len; ++i)
        {
            const float k = (float) (i + 1) / (float) len;
            const float m = mixPrev + (mixNew - mixPrev) * k;
            const float l = levelPrev + (lvlNew - levelPrev) * k;

            // Somme des voix : chacune reçoit le même signal d'entrée
            float wetSum[2] = { 0.0f, 0.0f };
            float driven[2] = { 0.0f, 0.0f };   // saturation d'entrée : une seule fois par canal, suréchantillonnée x2, puis partagée par les voix
            for (int c = 0; c < numCh; ++c)
                driven[c] = driveStage[c].process (data[c][start + i]);
            for (int v = 0; v < bbdres::kNumVoices; ++v)
            {
                if (! voiceActive[v]) continue;
                // sorties de boucle de l'échantillon précédent, pour le couplage entre les deux canaux de la voix
                const float crossIn[2] = { resonators[v][1].loopOut(), resonators[v][0].loopOut() };
                for (int c = 0; c < numCh; ++c)
                {
                    const float gain = c == 0 ? voiceGainL[v] : voiceGainR[v];
                    wetSum[c] += gain * resonators[v][c].processDriven (driven[c], crossIn[c]);
                }
            }

            // Lecture des mémoires : la boucle rappelée remplace peu à peu le son wet (Hold Mix règle la part)
            if (players[0].active() || players[1].active())
            {
                float out[2][2] = {};
                players[0].process (out[0][0], out[0][1]);
                players[1].process (out[1][0], out[1][1]);
                const float e0 = players[0].envelope(), e1 = players[1].envelope(), es = e0 + e1;
                if (es > 1.0e-6f)
                {
                    const float kk = t.holdMix * std::min (1.0f, es);
                    for (int c = 0; c < numCh; ++c)
                    {
                        const float a = numCh > 1 ? out[0][c] : 0.5f * (out[0][0] + out[0][1]);
                        const float b = numCh > 1 ? out[1][c] : 0.5f * (out[1][0] + out[1][1]);
                        wetSum[c] += ((a * e0 + b * e1) / es - wetSum[c]) * kk;
                    }
                }
            }
            if (cqtActive)   // analyseur : la sortie wet, avant le mélange avec le signal sec
            {
                cqtRing[cqtWrite & (unsigned) (kCqtRing - 1)].store (numCh > 1 ? 0.5f * (wetSum[0] + wetSum[1]) : wetSum[0], std::memory_order_relaxed);
                ++cqtWrite;
            }
            for (int c = 0; c < numCh; ++c)
            {
                float& s = data[c][start + i];
                const float dry = s;
                s = dry + (wetSum[c] * l - dry) * m;   // Level n'agit que sur le signal wet : dry * (1 - Mix) + wet * Level * Mix
            }
        }
        mixPrev = mixNew;
        levelPrev = lvlNew;
    }
    cqtPos.store (cqtWrite, std::memory_order_release);
    updateSlotStates();
}

void BBDResAudioProcessor::updateSlotStates()
{
    for (int i = 0; i < bbdres::kNumFreezeSlots; ++i)
    {
        int s = freezeBank.used (i) ? 1 : 0;
        if (bounceJob.pending.load (std::memory_order_relaxed) && bounceJob.slot == i) s = 2;   // mise en mémoire en cours
        else if ((players[0].active() && players[0].slot() == i) || (players[1].active() && players[1].slot() == i)) s = 3;
        uiSlotState[i].store (s, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* BBDResAudioProcessor::createEditor()
{
    return new BBDResAudioProcessorEditor (*this);
}

void BBDResAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        // Les boucles Freeze mémorisées vont dans l'état du PROJET uniquement (int16 stéréo, en base64) :
        // les presets du plug-in ne les contiennent jamais.
        auto* slots = xml->createNewChildElement ("FREEZESLOTS");
        for (int i = 0; i < bbdres::kNumFreezeSlots; ++i)
        {
            std::vector<float> l, r;
            float sr = 0.0f;
            int pre = 0;
            if (! freezeBank.exportSlot (i, l, r, sr, pre) || l.empty()) continue;
            const auto enc = bbdres::encodeSlot (l.data(), r.data(), (int) l.size());
            auto* e = slots->createNewChildElement ("SLOT");
            e->setAttribute ("index", i);
            e->setAttribute ("sr", (double) sr);
            e->setAttribute ("pre", pre);
            e->setAttribute ("data", juce::MemoryBlock (enc.data(), enc.size() * sizeof (std::int16_t)).toBase64Encoding());
        }
        if (slots->getNumChildElements() == 0)
            xml->removeChildElement (slots, true);
        copyXmlToBinary (*xml, destData);
    }
}

void BBDResAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            freezeBank.clearAll();
            if (auto* slots = xml->getChildByName ("FREEZESLOTS"))   // boucles Freeze mémorisées avec ce projet
            {
                for (auto* e : slots->getChildWithTagNameIterator ("SLOT"))
                {
                    const int idx = e->getIntAttribute ("index", -1);
                    juce::MemoryBlock mb;
                    std::vector<float> l, r;
                    if (idx >= 0 && idx < bbdres::kNumFreezeSlots && mb.fromBase64Encoding (e->getStringAttribute ("data"))
                        && bbdres::decodeSlot (static_cast<const std::int16_t*> (mb.getData()), mb.getSize() / sizeof (std::int16_t), l, r))
                        freezeBank.load (idx, std::move (l), std::move (r), (float) e->getDoubleAttribute ("sr", 48000.0), e->getIntAttribute ("pre"));
                }
                xml->removeChildElement (slots, true);   // ne reste pas dans l'état des réglages
            }
            auto tree = juce::ValueTree::fromXml (*xml);
            if ((int) tree.getProperty ("layout", 1) < 2)   // projet enregistré avant 1/32 et 1/24 : les durées de pas ont glissé de deux rangs
            {
                for (int i = 0; i < tree.getNumChildren(); ++i)
                {
                    auto child = tree.getChild (i);
                    if (child.hasType ("PARAM") && child.getProperty ("id").toString() == ParamIDs::seqStep)
                        child.setProperty ("value", (float) child.getProperty ("value") + 2.0f, nullptr);
                }
                tree.setProperty ("layout", 2, nullptr);
            }
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BBDResAudioProcessor();
}

// Fil à part : rend la boucle gelée décrite par le fil audio, puis la range dans la mémoire demandée.
// Les voix actives sont mélangées avec leur niveau et leur panoramique du moment, avant les filtres du Hold.
void BBDResAudioProcessor::processBounceJob()
{
    if (! bounceJob.pending.load (std::memory_order_acquire)) return;
    std::vector<float> accL, accR;
    int length = 0, fade = 0;
    bool ok = true;
    for (int v = 0; v < bbdres::kNumVoices && ok; ++v)
    {
        const auto& jv = bounceJob.v[v];
        if (! jv.used) continue;
        if (length == 0)
        {
            length = jv.ex[0].L;
            fade = jv.ex[0].X;
            accL.assign ((size_t) length, 0.0f);
            accR.assign ((size_t) length, 0.0f);
        }
        if (jv.ex[0].L != length || jv.ex[1].L != length) { ok = false; break; }
        bbdres::renderLoopCycle (jv.ex[0], jv.gl, accL);
        bbdres::renderLoopCycle (jv.ex[1], jv.gr, accR);
        if (! jv.ex[0].stillValid() || ! jv.ex[1].stillValid()) ok = false;   // le Freeze a été refait pendant le rendu
    }
    if (ok && length > 0 && freezeBank.isPrepared() && ! freezeBank.used (bounceJob.slot))
        freezeBank.load (bounceJob.slot, bbdres::makeSlotBuffer (accL, fade), bbdres::makeSlotBuffer (accR, fade), bounceJob.fs, fade);
    bounceJob.pending.store (false, std::memory_order_release);
}
