#pragma once
 
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/Resonator.h"
 
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
 
    // Valeurs cibles lues dans les paramètres (avant lissage)
    struct Targets
    {
        float semis, feedback, inLog2, loopLog2, mix, level;
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
 
    bbdres::Resonator resonators[2];
    bbdres::Smoother tuneSm, fbSm, inSm, loopSm, mixSm, levelSm;
    float mixPrev = 0.5f, levelPrev = 1.0f;
 
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BBDResAudioProcessor)
};
 
