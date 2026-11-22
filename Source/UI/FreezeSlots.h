#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "../Params.h"
#include "../PluginProcessor.h"

namespace bbdui
{
// Quatre mémoires de boucles Freeze, en boutons (sous FREEZE). Chaque bouton plein montre la forme d'onde de sa boucle.
//  - clic long (0,5 s), pendant que Freeze tourne : enregistre la boucle gelée en cours. Le bouton scintille pendant
//    l'enregistrement (un tour de boucle) puis une dernière fois quand c'est fini ;
//  - clic court : rappelle la boucle (fondu vers elle ; Hold Mix règle la part de boucle) ; un nouveau clic dessus l'arrête
//    (sortie en 0,5 s) ;
//  - bouton plein = une boucle y est mémorisée, bouton creux = vide ; contour clair = en lecture.
// Les boucles sont sauvées avec le projet Live, jamais avec les presets du plug-in.
class FreezeSlotButtons : public juce::Component, private juce::Timer
{
public:
    explicit FreezeSlotButtons (BBDResAudioProcessor& p) : proc (p)
    {
        freezeP = p.apvts.getRawParameterValue (ParamIDs::freeze);
        for (auto& v : version) v = 0xffffffffu;
        for (auto& s : lastState) s = -1;
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (int i = 0; i < bbdres::kNumFreezeSlots; ++i)
        {
            const auto r = cell (i).toFloat();
            const int state = lastState[i];
            const bool recording = state == 2;
            const bool saved = now - flashStart[i] < 700u;
            const bool flash = (recording && ((now / 120u) % 2u) == 0u) || (saved && (((now - flashStart[i]) / 60u) % 2u) == 0u);
            const bool denied = now - deniedStart[i] < 350u;
            const bool used = state >= 1;

            g.setColour (flash ? kText : (used ? kAmber.withAlpha (0.85f) : kDark));
            g.fillRoundedRectangle (r, 4.0f);
            if (used && ! flash)   // forme d'onde de la boucle
            {
                g.setColour (kBg.withAlpha (0.85f));
                const int w = (int) r.getWidth() - 6;
                for (int x = 0; x < w && x < (int) peaks[i].size(); ++x)
                {
                    const float a = juce::jmin (1.0f, peaks[i][(size_t) x] * 1.6f) * (r.getHeight() * 0.5f - 3.0f) + 0.5f;
                    g.drawVerticalLine ((int) r.getX() + 3 + x, r.getCentreY() - a, r.getCentreY() + a);
                }
            }
            g.setColour (denied ? juce::Colour (0xffe0806a) : (state == 3 ? kText : kLine));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, (state == 3 || denied) ? 1.6f : 1.0f);
            if (! used)
            {
                g.setColour (kMuted);
                g.setFont (font (11.0f, true));
                g.drawText (juce::String (i + 1), r.toNearestInt(), juce::Justification::centred);
            }
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
        if (pressed >= 0 && ! longFired && lastState[pressed] >= 1)   // clic court : rappel (ou arrêt)
            proc.requestFreezeSlot (1, pressed);
        pressed = -1;
    }

private:
    juce::Rectangle<int> cell (int i) const
    {
        const int gap = 4;
        const int w = (getWidth() - 3 * gap) / bbdres::kNumFreezeSlots;
        return { i * (w + gap), 0, w, getHeight() };
    }

    int cellAt (juce::Point<int> p) const
    {
        for (int i = 0; i < bbdres::kNumFreezeSlots; ++i)
            if (cell (i).contains (p)) return i;
        return -1;
    }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (pressed >= 0 && ! longFired && now - pressStart >= 500u)   // clic long : enregistrement
        {
            longFired = true;
            if (freezeP->load() > 0.5f) proc.requestFreezeSlot (0, pressed);   // il faut que Freeze tourne
            else                        deniedStart[pressed] = now;            // sinon : bref contour rouge
        }
        for (int i = 0; i < bbdres::kNumFreezeSlots; ++i)
        {
            const int s = proc.uiSlotState[i].load (std::memory_order_relaxed);
            if (lastState[i] == 2 && s != 2 && s >= 1) flashStart[i] = now;   // fin d'enregistrement : dernier scintillement
            lastState[i] = s;
            const unsigned v = proc.freezeBank.version (i);
            if (v != version[i])   // contenu modifié : forme d'onde à recalculer
            {
                version[i] = v;
                peaks[i] = s >= 1 ? proc.freezeBank.peaks (i, juce::jmax (8, cell (i).getWidth() - 6)) : std::vector<float>();
            }
        }
        repaint();
    }

    BBDResAudioProcessor& proc;
    std::atomic<float>* freezeP = nullptr;
    std::vector<float> peaks[bbdres::kNumFreezeSlots];
    unsigned version[bbdres::kNumFreezeSlots];
    int lastState[bbdres::kNumFreezeSlots];
    juce::uint32 flashStart[bbdres::kNumFreezeSlots] = {}, deniedStart[bbdres::kNumFreezeSlots] = {};
    int pressed = -1;
    bool longFired = false;
    juce::uint32 pressStart = 0;
};
} // namespace bbdui
