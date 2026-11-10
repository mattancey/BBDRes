#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "../Params.h"
#include "../PluginProcessor.h"
#include "../Dsp/Sequencer.h"

namespace bbdui
{
// Grille des 16 pas du séquenceur : une barre par pas (hauteur en demi-tons par rapport à Tune,
// de -24 à +24), un point d'activation dessous. Glisser sur les barres les règle (comme au crayon),
// cliquer sur le point active ou désactive le pas, double-clic remet le pas à zéro.
// Le pas en cours est éclairé ; les pas au-delà de la longueur choisie sont estompés.
class StepGrid : public juce::Component, private juce::Timer
{
public:
    StepGrid (juce::AudioProcessorValueTreeState& a, BBDResAudioProcessor& p) : proc (p)
    {
        for (int i = 0; i < bbdres::kMaxSeqSteps; ++i)
        {
            valueParam[i] = a.getParameter (ParamIDs::kStepIDs[i]);
            onParam[i] = a.getParameter (ParamIDs::kStepOnIDs[i]);
            valueAttach[i] = std::make_unique<juce::ParameterAttachment> (*valueParam[i], [this] (float) { repaint(); });
            onAttach[i] = std::make_unique<juce::ParameterAttachment> (*onParam[i], [this] (float) { repaint(); });
            valueAttach[i]->sendInitialUpdate();
            onAttach[i]->sendInitialUpdate();
        }
        lengthP = a.getRawParameterValue (ParamIDs::seqLength);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        const float colW = (float) getWidth() / (float) bbdres::kMaxSeqSteps;
        const int length = juce::jlimit (1, bbdres::kMaxSeqSteps, juce::roundToInt (lengthP->load()));
        const int current = proc.uiSeqStep.load();
        const float barTop = 16.0f, barBottom = (float) getHeight() - 26.0f;
        const float barH = barBottom - barTop, yZero = barTop + barH * 0.5f;

        g.setColour (kBorder);
        g.drawLine (0.0f, yZero, (float) getWidth(), yZero, 1.0f);

        for (int i = 0; i < bbdres::kMaxSeqSteps; ++i)
        {
            const float x = (float) i * colW;
            const bool inLength = i < length;
            const float fade = inLength ? 1.0f : 0.3f;
            const float value = valueParam[i]->convertFrom0to1 (valueParam[i]->getValue());
            const bool isOn = onParam[i]->getValue() > 0.5f;

            if (i == current)
            {
                g.setColour (kAmber.withAlpha (0.14f));
                g.fillRoundedRectangle (x + 1.0f, 0.0f, colW - 2.0f, (float) getHeight(), 4.0f);
            }

            // barre de hauteur
            const float yVal = yZero - (value / 24.0f) * (barH * 0.5f);
            const auto bar = juce::Rectangle<float> (x + colW * 0.22f, std::min (yZero, yVal), colW * 0.56f, std::max (2.0f, std::abs (yZero - yVal)));
            g.setColour ((isOn ? kAmber : kLine).withAlpha (fade));
            g.fillRoundedRectangle (bar, 2.0f);

            // valeur
            g.setColour (kMuted.withAlpha (fade));
            g.setFont (font (9.0f));
            const int n = juce::roundToInt (value);
            g.drawText ((n > 0 ? "+" : "") + juce::String (n), (int) x, 0, (int) colW, 14, juce::Justification::centred);

            // point d'activation
            const float cx = x + colW * 0.5f, cy = (float) getHeight() - 12.0f;
            g.setColour ((isOn ? kAmber : kLine).withAlpha (fade));
            if (isOn) g.fillEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
            else      g.drawEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int col = columnAt (e.x);
        if ((float) e.y >= (float) getHeight() - 24.0f)
        {
            const bool isOn = onParam[col]->getValue() > 0.5f;
            onAttach[col]->setValueAsCompleteGesture (isOn ? 0.0f : 1.0f);
            return;
        }
        setStepFromMouse (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if ((float) e.y < (float) getHeight() - 24.0f) setStepFromMouse (e);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if ((float) e.y < (float) getHeight() - 24.0f)
            valueAttach[columnAt (e.x)]->setValueAsCompleteGesture (0.0f);
    }

private:
    int columnAt (int x) const
    {
        return juce::jlimit (0, bbdres::kMaxSeqSteps - 1, (int) ((float) x / ((float) getWidth() / (float) bbdres::kMaxSeqSteps)));
    }

    void setStepFromMouse (const juce::MouseEvent& e)
    {
        const int col = columnAt (e.x);
        const float barTop = 16.0f, barBottom = (float) getHeight() - 26.0f;
        const float norm = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - barTop) / (barBottom - barTop));
        const float semis = std::round (norm * 48.0f - 24.0f);
        valueAttach[col]->setValueAsCompleteGesture (semis);
    }

    void timerCallback() override
    {
        const int current = proc.uiSeqStep.load();
        const int length = juce::roundToInt (lengthP->load());
        if (current != lastCurrent || length != lastLength)
        {
            lastCurrent = current;
            lastLength = length;
            repaint();
        }
    }

    BBDResAudioProcessor& proc;
    juce::RangedAudioParameter* valueParam[bbdres::kMaxSeqSteps] = {};
    juce::RangedAudioParameter* onParam[bbdres::kMaxSeqSteps] = {};
    std::unique_ptr<juce::ParameterAttachment> valueAttach[bbdres::kMaxSeqSteps];
    std::unique_ptr<juce::ParameterAttachment> onAttach[bbdres::kMaxSeqSteps];
    std::atomic<float>* lengthP = nullptr;
    int lastCurrent = -2, lastLength = -1;
};
} // namespace bbdui
