#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "../Params.h"
#include "../PluginProcessor.h"
#include "../Dsp/Pitch.h"
#include "../Dsp/Response.h"

// Vues de l'interface : clavier de gamme et courbe de réponse du résonateur.
namespace bbdui
{
// Deux octaves de clavier : les notes de la gamme choisie sont éclairées, la tonique est plus vive,
// la note jouée (Tune, quantifiée si Quantize est actif) porte un point. Un clic règle Tune.
class KeyboardView : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardView (juce::AudioProcessorValueTreeState& a)
        : tuneParam (*a.getParameter (ParamIDs::tune)), tuneAttach (tuneParam, [] (float) {})
    {
        tuneP = a.getRawParameterValue (ParamIDs::tune);
        rootP = a.getRawParameterValue (ParamIDs::root);
        scaleP = a.getRawParameterValue (ParamIDs::scale);
        quantP = a.getRawParameterValue (ParamIDs::quantize);
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        const State s = readState();
        const float w = (float) getWidth() / 14.0f, h = (float) getHeight();
        static const int whiteSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int blackSemis[5] = { 1, 3, 6, 8, 10 };
        static const int blackAfter[5] = { 0, 1, 3, 4, 5 };

        for (int i = 0; i < 14; ++i)
        {
            const int note = s.start + (i / 7) * 12 + whiteSemis[i % 7];
            const auto key = juce::Rectangle<float> ((float) i * w + 1.0f, 0.0f, w - 2.0f, h);
            g.setColour (keyColour (s, note, false));
            g.fillRoundedRectangle (key, 2.0f);
            if (note == s.played) drawMarker (g, key.getCentreX(), h - 9.0f);
        }
        for (int oct = 0; oct < 2; ++oct)
            for (int j = 0; j < 5; ++j)
            {
                const int note = s.start + oct * 12 + blackSemis[j];
                const float bw = w * 0.62f;
                const float x = (float) (blackAfter[j] + oct * 7 + 1) * w - bw * 0.5f;
                const auto key = juce::Rectangle<float> (x, 0.0f, bw, h * 0.62f);
                g.setColour (keyColour (s, note, true));
                g.fillRoundedRectangle (key, 2.0f);
                if (note == s.played) drawMarker (g, key.getCentreX(), key.getBottom() - 8.0f);
            }
    }

    void mouseDown (const juce::MouseEvent& e) override { pick (e); }
    void mouseDrag (const juce::MouseEvent& e) override { pick (e); }

private:
    struct State { int start, played, root; unsigned mask; bool quant; };

    State readState() const
    {
        State s;
        const float tune = tuneP->load();
        s.root = juce::jlimit (0, 11, juce::roundToInt (rootP->load()));
        s.mask = bbdres::kScales[juce::jlimit (0, bbdres::kNumScales - 1, juce::roundToInt (scaleP->load()))].mask;
        s.quant = quantP->load() > 0.5f;
        s.played = s.quant ? juce::roundToInt (bbdres::quantizeToScale (tune, s.root, s.mask)) : juce::roundToInt (tune);
        s.start = 12 * (int) std::floor ((float) s.played / 12.0f);
        return s;
    }

    static bool inScale (const State& s, int note)
    {
        const int degree = (((note - s.root) % 12) + 12) % 12;
        return ((s.mask >> degree) & 1u) != 0u;
    }

    static juce::Colour keyColour (const State& s, int note, bool black)
    {
        const bool isRoot = (((note - s.root) % 12) + 12) % 12 == 0;
        if (inScale (s, note))
            return isRoot ? kAmber.brighter (0.35f) : (black ? kAmber.darker (0.35f) : kAmber);
        return black ? juce::Colour (0xff2a2d31) : juce::Colour (0xffcfc9ba);
    }

    static void drawMarker (juce::Graphics& g, float cx, float cy)
    {
        g.setColour (kText);
        g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour (kBg);
        g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    void pick (const juce::MouseEvent& e)
    {
        const State s = readState();
        const float w = (float) getWidth() / 14.0f, h = (float) getHeight();
        static const int whiteSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int blackSemis[5] = { 1, 3, 6, 8, 10 };
        static const int blackAfter[5] = { 0, 1, 3, 4, 5 };
        int note = -1;
        if ((float) e.y < h * 0.62f)   // les touches noires d'abord : elles sont au-dessus
            for (int oct = 0; oct < 2 && note < 0; ++oct)
                for (int j = 0; j < 5; ++j)
                {
                    const float bw = w * 0.62f, x = (float) (blackAfter[j] + oct * 7 + 1) * w - bw * 0.5f;
                    if ((float) e.x >= x && (float) e.x < x + bw) { note = s.start + oct * 12 + blackSemis[j]; break; }
                }
        if (note < 0)
        {
            const int i = juce::jlimit (0, 13, (int) ((float) e.x / w));
            note = s.start + (i / 7) * 12 + whiteSemis[i % 7];
        }
        tuneAttach.setValueAsCompleteGesture (tuneParam.convertFrom0to1 (tuneParam.convertTo0to1 ((float) note)));
        repaint();
    }

    void timerCallback() override
    {
        const State s = readState();
        const int stamp = s.played * 4096 + s.root * 256 + (int) (s.mask & 0xffu) + (s.quant ? 1 << 20 : 0) + (int) ((s.mask >> 8) << 24);
        if (stamp != lastStamp) { lastStamp = stamp; repaint(); }
    }

    juce::RangedAudioParameter& tuneParam;
    juce::ParameterAttachment tuneAttach;
    std::atomic<float>* tuneP = nullptr;
    std::atomic<float>* rootP = nullptr;
    std::atomic<float>* scaleP = nullptr;
    std::atomic<float>* quantP = nullptr;
    int lastStamp = -1;
};

// Courbe de réponse du résonateur (sortie humide) : pics aux multiples de la note, creusés par
// Feedback, filtrés par Input LP, Loop LP et le BBD. Calculée d'après les réglages (sans les modulations).
class ResponseView : public juce::Component, private juce::Timer
{
public:
    ResponseView (juce::AudioProcessorValueTreeState& a, BBDResAudioProcessor& p) : proc (p)
    {
        tuneP = a.getRawParameterValue (ParamIDs::tune);
        fineP = a.getRawParameterValue (ParamIDs::fine);
        fbP = a.getRawParameterValue (ParamIDs::feedback);
        inP = a.getRawParameterValue (ParamIDs::inputLP);
        loopP = a.getRawParameterValue (ParamIDs::loopLP);
        bbdP = a.getRawParameterValue (ParamIDs::bbd);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (kDark);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        const float left = 40.0f, right = 14.0f, top = 14.0f, bottom = 24.0f;
        const float pw = r.getWidth() - left - right, ph = r.getHeight() - top - bottom;
        const float fMin = 40.0f, fMax = 12000.0f, dbMin = -30.0f, dbMax = 30.0f;
        auto xOf = [&] (float f) { return left + std::log (f / fMin) / std::log (fMax / fMin) * pw; };
        auto yOf = [&] (float db) { return top + (1.0f - (db - dbMin) / (dbMax - dbMin)) * ph; };

        // grille et repères
        g.setFont (font (9.0f));
        for (const float f : { 100.0f, 1000.0f, 10000.0f })
        {
            g.setColour (kBorder);
            g.drawLine (xOf (f), top, xOf (f), top + ph, 1.0f);
            g.setColour (kMuted);
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                        (int) xOf (f) - 20, (int) (top + ph + 4.0f), 40, 14, juce::Justification::centred);
        }
        for (const float db : { -20.0f, 0.0f, 20.0f })
        {
            g.setColour (db == 0.0f ? kLine : kBorder);
            g.drawLine (left, yOf (db), left + pw, yOf (db), 1.0f);
            g.setColour (kMuted);
            g.drawText ((db > 0.0f ? "+" : "") + juce::String ((int) db) + " dB", 2, (int) yOf (db) - 7, (int) left - 6, 14, juce::Justification::centredRight);
        }

        // modèle d'après les réglages actuels
        bbdres::ResponseParams rp;
        rp.semitones = tuneP->load() + fineP->load() * 0.01f;
        rp.feedback = fbP->load();
        rp.inputLpHz = inP->load();
        rp.loopLpHz = loopP->load();
        rp.bbd = bbdP->load();
        const double sr = proc.getSampleRate();
        rp.fs = sr > 8000.0 ? (float) sr : 48000.0f;
        bbdres::ResponseModel model;
        model.set (rp);

        juce::Path curve;
        const int n = juce::jmax (2, (int) pw);
        for (int i = 0; i < n; ++i)
        {
            const float f = fMin * std::pow (fMax / fMin, (float) i / (float) (n - 1));
            const float db = juce::jlimit (dbMin, dbMax, model.db (f));
            if (i == 0) curve.startNewSubPath (left, yOf (db));
            else        curve.lineTo (left + (float) i * pw / (float) (n - 1), yOf (db));
        }
        juce::Path fill (curve);
        fill.lineTo (left + pw, yOf (dbMin));
        fill.lineTo (left, yOf (dbMin));
        fill.closeSubPath();
        g.setColour (kAmber.withAlpha (0.10f));
        g.fillPath (fill);
        g.setColour (kAmber);
        g.strokePath (curve, juce::PathStrokeType (1.6f));

        // note fondamentale
        const float f0 = model.fundamentalHz();
        if (f0 > fMin && f0 < fMax)
        {
            g.setColour (kAmber.withAlpha (0.6f));
            g.drawLine (xOf (f0), top, xOf (f0), top + ph, 1.0f);
        }
    }

private:
    void timerCallback() override
    {
        const float stamp = tuneP->load() * 1000.0f + fineP->load() * 3.0f + fbP->load() * 71.0f
                          + std::log2 (inP->load()) * 13.0f + std::log2 (loopP->load()) * 17.0f + bbdP->load() * 29.0f
                          + (float) proc.getSampleRate() * 0.001f;
        if (std::abs (stamp - lastStamp) > 1.0e-4f) { lastStamp = stamp; repaint(); }
    }

    BBDResAudioProcessor& proc;
    std::atomic<float>* tuneP = nullptr;
    std::atomic<float>* fineP = nullptr;
    std::atomic<float>* fbP = nullptr;
    std::atomic<float>* inP = nullptr;
    std::atomic<float>* loopP = nullptr;
    std::atomic<float>* bbdP = nullptr;
    float lastStamp = -1.0e9f;
};
} // namespace bbdui
