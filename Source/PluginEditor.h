#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/BBDLookAndFeel.h"

// Interface de BBDRes V2 : une surface de 1280 x 780 points, redimensionnable à proportions fixes.
class BBDResAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit BBDResAudioProcessorEditor (BBDResAudioProcessor&);
    ~BBDResAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kDesignW = 1280;
    static constexpr int kDesignH = 906;

    bbdui::BBDLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::Component> content;
    BBDResAudioProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BBDResAudioProcessorEditor)
};
