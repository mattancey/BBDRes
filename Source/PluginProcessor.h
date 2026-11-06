#pragma once
 
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/Resonator.h"
#include "Dsp/Pitch.h"
#include "Dsp/Modulation.h"
 
class BBDResAudioProcessor : public juce::AudioProcessor
{
public:
    BBDResAudioProcessor();
    ~BBDResAudioProcessor() override = default;
 
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
 
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
 
    // Valeurs cibles lues dans les paramètres (avant modulation et lissage)
    struct Targets
    {
        float tune, fine, feedback, inLog2, loopLog2, mix, level, drive, bbd;
        float lfoRate, lfoDepth, dynamics, drift;
        int curve, root, scale, lfoShape, lfoTarget;
        bool quantize;
    };
    Targets readTargets() const;
 
    static constexpr int kControlBlock = 16;  // les paramètres sont mis à jour tous les 16 échantillons
 
    std::atomic<float>* tuneP = nullptr;
    std::atomic<float>* fineP = nullptr;
    std::atomic<float>* feedbackP = nullptr;
    std::atomic<float>* inputLpP = nullptr;
    std::atomic<float>* loopLpP = nullptr;
    std::atomic<float>* mixP = nullptr;
    std::atomic<float>* levelP = nullptr;
    std::atomic<float>* driveP = nullptr;
    std::atomic<float>* curveP = nullptr;
    std::atomic<float>* bbdP = nullptr;
    std::atomic<float>* rootP = nullptr;
    std::atomic<float>* scaleP = nullptr;
    std::atomic<float>* quantizeP = nullptr;
    std::atomic<float>* lfoShapeP = nullptr;
    std::atomic<float>* lfoRateP = nullptr;
    std::atomic<float>* lfoDepthP = nullptr;
    std::atomic<float>* lfoTargetP = nullptr;
    std::atomic<float>* dynamicsP = nullptr;
    std::atomic<float>* driftP = nullptr;
 
    bbdres::Resonator resonators[2];
    bbdres::Smoother tuneSm, fbSm, inSm, loopSm, mixSm, levelSm, driveSm, bbdSm;
    bbdres::Lfo lfo, driftTuneLfo, driftFilterLfo;
    bbdres::EnvFollower env;
    float mixPrev = 0.5f, levelPrev = 1.0f;
 
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BBDResAudioProcessor)
};
 
