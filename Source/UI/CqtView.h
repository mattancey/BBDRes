#pragma once

#include <cstring>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "Views.h"
#include "../PluginProcessor.h"
#include "../Dsp/Cqt.h"

namespace bbdui
{
// Analyseur CQT de la sortie wet (somme des voix, avant Mix), en cascade : le temps défile de droite à gauche,
// les hauteurs montent de do0 (en bas) à environ 8 kHz (en haut), 24 bins par octave.
// Teinte = classe de hauteur (do rouge, le cycle se répète à chaque octave), luminosité = niveau (noir profond sous -85 dB).
// Un triangle sur le bord droit marque la fondamentale de chaque résonateur actif, dans sa couleur.
// L'analyse ne tourne que tant que la fenêtre est ouverte : le processeur n'enregistre rien sinon.
class CqtView : public juce::Component, private juce::Timer
{
public:
    explicit CqtView (BBDResAudioProcessor& p) : proc (p)
    {
        setInterceptsMouseClicks (false, false);
        history = juce::Image (juce::Image::ARGB, kW, kH, true);
        db.assign ((size_t) bbdres::Cqt::kNumBins, -120.0f);
        proc.cqtOn.store (true);
        startTimerHz (20);
    }

    ~CqtView() override { proc.cqtOn.store (false); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::black);
        g.fillRect (kLabelW, 0, kW, kH);
        g.drawImageAt (history, kLabelW, 0);

        g.setFont (font (9.0f));
        for (int o = 0; o < bbdres::Cqt::kOctaves; ++o)   // un repère par do
        {
            const float y = rowOf ((float) (o * bbdres::Cqt::kBinsPerOctave));
            g.setColour (kLine.withAlpha (0.35f));
            g.drawHorizontalLine ((int) y, (float) kLabelW, (float) (kLabelW + kW));
            g.setColour (kMuted);
            g.drawText ("C" + juce::String (o), 0, (int) y - 6, kLabelW - 4, 12, juce::Justification::centredRight);
        }

        for (int v = 0; v < bbdres::kNumVoices; ++v)   // fondamentale de chaque résonateur actif
        {
            const float semis = proc.uiPitchVoice[v].load();
            if (semis < 0.0f) continue;
            const float hz = 440.0f * std::exp2 ((semis - 69.0f) / 12.0f);
            const float bin = (float) bbdres::Cqt::kBinsPerOctave * std::log2 (hz / bbdres::Cqt::kMinHz);
            if (bin < 0.0f || bin > (float) (bbdres::Cqt::kNumBins - 1)) continue;
            const float y = rowOf (bin), x = (float) (kLabelW + kW) - 1.0f;
            juce::Path tri;
            tri.addTriangle (x, y, x - 9.0f, y - 5.0f, x - 9.0f, y + 5.0f);
            g.setColour (kVoiceColours[v]);
            g.fillPath (tri);
        }
        g.setColour (kBorder);
        g.drawRect (kLabelW, 0, kW, kH, 1);
    }

private:
    static constexpr int kLabelW = 24, kW = 234, kH = 186;

    static float rowOf (float bin)
    {
        return (float) (kH - 1) * (1.0f - bin / (float) (bbdres::Cqt::kNumBins - 1));
    }

    void timerCallback() override
    {
        const double sr = proc.getSampleRate();
        if (sr < 8000.0) return;
        if ((float) sr != lastFs)   // (re)construit les noyaux quand la fréquence d'échantillonnage change
        {
            lastFs = (float) sr;
            cqt.prepare (lastFs);
            frame.assign ((size_t) cqt.maxLength(), 0.0f);
        }
        const int n = cqt.maxLength();
        const unsigned pos = proc.cqtPos.load (std::memory_order_acquire);
        constexpr unsigned mask = (unsigned) BBDResAudioProcessor::kCqtRing - 1u;
        for (int i = 0; i < n; ++i)
            frame[(size_t) i] = proc.cqtRing[(pos - (unsigned) n + (unsigned) i) & mask].load (std::memory_order_relaxed);
        cqt.analyze (frame.data(), n, db.data());
        pushColumn();
        repaint();
    }

    // fait défiler l'image d'un pixel vers la gauche et dessine la nouvelle colonne à droite
    void pushColumn()
    {
        juce::Image::BitmapData bd (history, juce::Image::BitmapData::readWrite);
        const int stride = bd.pixelStride;
        for (int y = 0; y < kH; ++y)
        {
            auto* line = bd.getLinePointer (y);
            std::memmove (line, line + stride, (size_t) ((kW - 1) * stride));
            const float pos = (float) (kH - 1 - y) / (float) (kH - 1) * (float) (bbdres::Cqt::kNumBins - 1);
            const int b0 = (int) pos, b1 = juce::jmin (b0 + 1, bbdres::Cqt::kNumBins - 1);
            const float fr = pos - (float) b0;
            const float level = db[(size_t) b0] + (db[(size_t) b1] - db[(size_t) b0]) * fr;
            const auto c = bbdres::cqtColour (fr < 0.5f ? b0 : b1, level);
            reinterpret_cast<juce::PixelARGB*> (line + (kW - 1) * stride)->setARGB (255, c.r, c.g, c.b);
        }
    }

    BBDResAudioProcessor& proc;
    bbdres::Cqt cqt;
    std::vector<float> frame, db;
    juce::Image history;
    float lastFs = 0.0f;
};
} // namespace bbdui
