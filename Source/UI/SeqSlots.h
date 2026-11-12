#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "../Presets.h"

namespace bbdui
{
// Quatre mémoires de séquence, en boutons 2 x 2.
//  - clic court : rappelle la séquence mémorisée (les pas, la longueur, la durée, le glide, Q SCALE) ;
//  - clic long (0,5 s) : mémorise la séquence en cours ; le bouton scintille pour le confirmer.
// Un bouton plein (ambre) contient une séquence, un bouton creux est vide.
// Les mémoires sont sauvées avec le projet et avec les presets généraux.
class SeqSlotButtons : public juce::Component, private juce::Timer
{
public:
    explicit SeqSlotButtons (bbdpresets::PresetManager& m) : manager (m)
    {
        for (auto& u : used) u = false;
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (int i = 0; i < bbdpresets::kNumSeqSlots; ++i)
        {
            const auto r = cell (i).toFloat();
            const bool flickering = i == flickerIndex && (now - flickerStart) < 700u;
            const bool flash = flickering && (((now - flickerStart) / 60u) % 2u) == 0u;

            g.setColour (flash ? kText : (used[i] ? kAmber.withAlpha (0.85f) : kDark));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (i == lastUsed ? kText : kLine);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, i == lastUsed ? 1.5f : 1.0f);
            g.setColour ((used[i] || flash) ? kBg : kMuted);
            g.setFont (font (12.0f, true));
            g.drawText (juce::String (i + 1), r.toNearestInt(), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pressed = cellAt (e.getPosition());
        longFired = false;
        pressStart = juce::Time::getMillisecondCounter();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (pressed >= 0 && ! longFired && manager.recallSeqSlot (pressed))   // clic court : rappel
            lastUsed = pressed;
        pressed = -1;
        repaint();
    }

private:
    juce::Rectangle<int> cell (int i) const
    {
        const int w = (getWidth() - 4) / 2, h = (getHeight() - 4) / 2;
        return { (i % 2) * (w + 4), (i / 2) * (h + 4), w, h };
    }

    int cellAt (juce::Point<int> p) const
    {
        for (int i = 0; i < bbdpresets::kNumSeqSlots; ++i)
            if (cell (i).contains (p)) return i;
        return -1;
    }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (pressed >= 0 && ! longFired && now - pressStart >= 500u)   // clic long : mémorisation
        {
            longFired = true;
            manager.saveSeqSlot (pressed);
            flickerIndex = pressed;
            flickerStart = now;
            lastUsed = pressed;
        }
        for (int i = 0; i < bbdpresets::kNumSeqSlots; ++i)
            used[i] = manager.slotUsed (i);
        repaint();
    }

    bbdpresets::PresetManager& manager;
    bool used[bbdpresets::kNumSeqSlots];
    int pressed = -1, flickerIndex = -1, lastUsed = -1;
    bool longFired = false;
    juce::uint32 pressStart = 0, flickerStart = 0;
};
} // namespace bbdui
