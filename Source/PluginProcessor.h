#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/Resonator.h"
#include "Dsp/Pitch.h"
#include "Dsp/Modulation.h"
#include "Dsp/Sequencer.h"
#include "Dsp/Voices.h"
#include "Dsp/FreezeSlots.h"
#include "Dsp/LoopPlayer.h"
#include "Dsp/LoopRender.h"
#include "Presets.h"

class BBDResAudioProcessor : public juce::AudioProcessor
{
public:
    BBDResAudioProcessor();
    ~BBDResAudioProcessor() override { bounceThread.stopThread (4000); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<bbdpresets::PresetManager> presets;   // presets généraux et de séquence, A/B
    std::atomic<float> uiBpm { 120.0f };   // tempo de l'hôte, lu par l'interface (affichage de la division)
    std::atomic<int> uiSeqStep { -1 };     // pas en cours du séquenceur (-1 = arrêté), lu par l'interface
    std::atomic<float> uiPitchVoice[bbdres::kNumVoices];   // hauteur de chaque voix (-1 = inactive), lue par l'interface
    // Sortie wet pour l'analyseur CQT de l'interface : somme mono des voix, avant Mix. Écrite seulement si l'interface est ouverte.
    static constexpr int kCqtRing = 131072;
    std::atomic<float> cqtRing[kCqtRing];
    std::atomic<unsigned> cqtPos { 0 };
    std::atomic<bool> cqtOn { false };
    // Mémoires de boucles Freeze : enregistrées avec le PROJET (état du plug-in), jamais avec les presets
    bbdres::FreezeSlotBank freezeBank;
    std::atomic<int> uiSlotState[bbdres::kNumFreezeSlots];   // 0 vide, 1 pleine, 2 enregistrement, 3 lecture
    // 0 enregistrer (mémoire vide : exporte la boucle gelée telle qu'elle joue), 1 rappeler / arrêter, 2 tout arrêter, 3 effacer
    void requestFreezeSlot (int type, int slot) { slotCommand.store (type * 8 + slot, std::memory_order_release); }
    std::atomic<float> uiPitch { 48.0f };  // hauteur réelle du résonateur (LFO, quantification), lue par l'interface

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Valeurs cibles lues dans les paramètres (avant modulation et lissage)
    struct Targets
    {
        float tune, fine, feedback, inLog2, loopLog2, mix, level, drive, bbd, noise;
        float lfoRate, lfoOffset, lfoDepth, loopDyn, drift, spread, cross, holdLen, holdMix, holdPhase, holdHpLog2, holdLpLog2, lowCutLog2, pultec;
        int curve, root, scale, lfoShape, lfoTarget, lfoSync;
        bool quantize, freeze, holdQuant, seqOn, seqQuant;
        int seqLength, seqStep;
        float seqGlide, stepVal[16];
        bool stepOn[16];
        bool voiceOn[bbdres::kNumVoices], voiceSolo[bbdres::kNumVoices];
        float voiceInt[bbdres::kNumVoices], voiceLvl[bbdres::kNumVoices], voicePan[bbdres::kNumVoices];
    };
    Targets readTargets() const;
    float voiceTargetGain (const Targets& t, int v) const;   // gain visé d'une voix : niveau, On, Solo
    bool voiceWasActive[bbdres::kNumVoices] = {};

    static constexpr int kControlBlock = 16;  // les paramètres sont mis à jour tous les 16 échantillons

    std::atomic<float>* tuneP = nullptr;
    std::atomic<float>* fineP = nullptr;
    std::atomic<float>* feedbackP = nullptr;
    std::atomic<float>* inputLpP = nullptr;
    std::atomic<float>* loopLpP = nullptr;
    std::atomic<float>* loopDynP = nullptr;
    std::atomic<float>* mixP = nullptr;
    std::atomic<float>* levelP = nullptr;
    std::atomic<float>* driveP = nullptr;
    std::atomic<float>* curveP = nullptr;
    std::atomic<float>* bbdP = nullptr;
    std::atomic<float>* noiseP = nullptr;
    std::atomic<float>* freezeP = nullptr;
    std::atomic<float>* holdLenP = nullptr;
    std::atomic<float>* holdQuantP = nullptr;
    std::atomic<float>* holdMixP = nullptr;
    std::atomic<float>* holdPhaseP = nullptr;
    std::atomic<float>* holdHpP = nullptr;
    std::atomic<float>* holdLpP = nullptr;
    std::atomic<float>* lowCutP = nullptr;
    std::atomic<float>* pultecP = nullptr;
    std::atomic<float>* seqOnP = nullptr;
    std::atomic<float>* seqLengthP = nullptr;
    std::atomic<float>* seqStepP = nullptr;
    std::atomic<float>* seqQuantP = nullptr;
    std::atomic<float>* seqGlideP = nullptr;
    std::atomic<float>* stepP[16] = {};
    std::atomic<float>* stepOnP[16] = {};
    std::atomic<float>* voiceOnP[bbdres::kNumVoices] = {};
    std::atomic<float>* voiceIntP[bbdres::kNumVoices] = {};
    std::atomic<float>* voiceLvlP[bbdres::kNumVoices] = {};
    std::atomic<float>* voicePanP[bbdres::kNumVoices] = {};
    std::atomic<float>* voiceSoloP[bbdres::kNumVoices] = {};
    std::atomic<float>* lfoPhaseP = nullptr;
    std::atomic<float>* rootP = nullptr;
    std::atomic<float>* scaleP = nullptr;
    std::atomic<float>* quantizeP = nullptr;
    std::atomic<float>* lfoShapeP = nullptr;
    std::atomic<float>* lfoSyncP = nullptr;
    std::atomic<float>* lfoRateP = nullptr;
    std::atomic<float>* lfoDepthP = nullptr;
    std::atomic<float>* lfoTargetP = nullptr;
    std::atomic<float>* driftP = nullptr;
    std::atomic<float>* spreadP = nullptr;
    std::atomic<float>* crossP = nullptr;

    double currentSampleRate = 44100.0;
    std::atomic<int> slotCommand { -1 };
    // Mise en mémoire d'une boucle : le fil audio décrit la boucle gelée (sans rien copier), un fil à part la rend
    struct BounceVoice { bbdres::LoopExport ex[2]; float gl = 0.0f, gr = 0.0f; bool used = false; };
    struct BounceJob
    {
        BounceVoice v[bbdres::kNumVoices];
        int slot = -1;
        float fs = 48000.0f;
        std::atomic<bool> pending { false };
    } bounceJob;
    struct BounceThread : juce::Thread
    {
        BounceThread() : juce::Thread ("DarkSwan freeze bounce") {}
        std::function<void()> work;
        void run() override { while (! threadShouldExit()) { if (work) work(); wait (15); } }
    } bounceThread;
    void processBounceJob();
    bbdres::LoopPlayer players[2];   // deux lecteurs : un nouveau rappel fond vers le nouveau pendant que l'ancien s'éteint
    int activePlayer = 0;
    bbdres::Smoother holdPhaseSm;
    void updateSlotStates();
    bbdres::SeqTransition seqTrans;   // glide d'environ 10 % d'un pas quand le contenu de la séquence change
    unsigned lastSeqHash = 0;
    bool seqHashValid = false;
    float lastSeqBase = 48.0f;
    double seqPosAcc = 0.0;      // position du séquenceur (en pas) quand le transport est à l'arrêt
    double syncPhaseAcc = 0.0;   // phase du LFO synchronisé quand le transport est à l'arrêt

    bbdres::Resonator resonators[bbdres::kNumVoices][2];   // 4 voix en parallèle, chacune en stéréo
    bbdres::Smoother fbSm, inSm, loopSm, mixSm, levelSm, driveSm, bbdSm,
                     noiseSm, spreadSm, crossSm, holdHpSm, holdLpSm, lowCutSm, pultecSm;
    bbdres::Smoother pitchSm[bbdres::kNumVoices], voiceGainSm[bbdres::kNumVoices], voicePanSm[bbdres::kNumVoices];
    bbdres::Lfo lfo, driftTuneLfo[bbdres::kNumVoices][2], driftFilterLfo[bbdres::kNumVoices][2];
    bbdres::EnvFollower env;
    bbdres::NoiseGate noiseGate;
    float mixPrev = 0.5f, levelPrev = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BBDResAudioProcessor)
};
