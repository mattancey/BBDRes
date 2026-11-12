#include "PluginEditor.h"
#include "Params.h"
#include "UI/Widgets.h"
#include "UI/Views.h"
#include "UI/StepGrid.h"
#include "UI/PresetBar.h"
#include "UI/SeqSlots.h"

namespace
{
using namespace bbdui;

void dashedBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text)
{
    juce::Path outline, dashed;
    outline.addRoundedRectangle (r, 6.0f);
    const float dashes[] = { 4.0f, 4.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (kLine);
    g.strokePath (dashed, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff7d838a));
    g.setFont (font (10.0f));
    g.drawText (text, r.toNearestInt(), juce::Justification::centred);
}

void panel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (kMuted);
    g.setFont (font (10.0f, true));
    g.drawText (title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 14, juce::Justification::centredLeft);
}

juce::String durationText (float seconds)
{
    return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms" : juce::String (seconds, 2) + " s";
}

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (BBDResAudioProcessor& p) : proc (p)
    {
        auto& a = proc.apvts;
        holdQuantP = a.getRawParameterValue (ParamIDs::holdQuant);

        // --- Input ---
        add<Knob> (a, ParamIDs::drive, "DRIVE").setBounds (28, 98, 64, 88);
        add<ChoiceBox> (a, ParamIDs::driveCurve, "CURVE").setBounds (108, 116, 76, 42);
        add<Knob> (a, ParamIDs::inputLP, "IN LP 12").setBounds (196, 98, 64, 88);

        // --- Pitch ---
        add<ChoiceBox> (a, ParamIDs::root, "ROOT").setBounds (28, 232, 70, 42);
        add<ChoiceBox> (a, ParamIDs::scale, "SCALE").setBounds (104, 232, 100, 42);
        add<ToggleBox> (a, ParamIDs::quantize, "QUANT").setBounds (212, 232, 62, 28);
        add<KeyboardView> (a, proc).setBounds (28, 284, 246, 60);

        // --- Resonator ---
        add<Knob> (a, ParamIDs::tune, "TUNE").setBounds (310, 104, 116, 150);
        add<Knob> (a, ParamIDs::fine, "FINE").setBounds (434, 138, 58, 88);
        add<Knob> (a, ParamIDs::feedback, "FEEDBACK").setBounds (494, 130, 58, 88);
        add<Knob> (a, ParamIDs::loopLP, "LOOP LP 6").setBounds (560, 130, 58, 88);
        add<Knob> (a, ParamIDs::loopDyn, "LOOP DYN").setBounds (620, 130, 58, 88);
        add<Knob> (a, ParamIDs::lowCut, "LOW CUT").setBounds (686, 130, 58, 88);
        pultecKnob = &add<Knob> (a, ParamIDs::pultec, "PULTEC");
        pultecKnob->setBounds (746, 130, 58, 88);
        add<Knob> (a, ParamIDs::bbd, "BBD").setBounds (812, 130, 58, 88);
        add<Knob> (a, ParamIDs::noise, "NOISE").setBounds (872, 130, 58, 88);
        add<ResponseView> (a, proc).setBounds (310, 272, 630, 210);

        // --- Mod ---
        add<ChoiceBox> (a, ParamIDs::lfoShape, "SHAPE").setBounds (976, 100, 92, 42);
        syncBox = &add<ChoiceBox> (a, ParamIDs::lfoSync, "SYNC");
        syncBox->setBounds (1074, 100, 80, 42);
        add<ChoiceBox> (a, ParamIDs::lfoTarget, "TARGET").setBounds (1160, 100, 92, 42);
        // Même emplacement pour deux potards : RATE en mode libre, PHASE en mode synchronisé
        rateKnob = &add<Knob> (a, ParamIDs::lfoRate, "RATE");
        rateKnob->setBounds (976, 156, 64, 88);
        phaseKnob = &add<Knob> (a, ParamIDs::lfoPhase, "PHASE");
        phaseKnob->setBounds (976, 156, 64, 88);
        add<Knob> (a, ParamIDs::lfoDepth, "DEPTH").setBounds (1056, 156, 64, 88);
        add<Knob> (a, ParamIDs::drift, "DRIFT").setBounds (1136, 156, 64, 88);

        // --- Stereo ---
        add<Knob> (a, ParamIDs::spread, "SPREAD").setBounds (976, 288, 64, 88);
        add<Knob> (a, ParamIDs::cross, "CROSS").setBounds (1056, 288, 64, 88);

        // --- Hold (looper) ---
        add<ToggleBox> (a, ParamIDs::freeze, "FREEZE").setBounds (30, 640, 110, 52);
        lengthKnob = &add<Knob> (a, ParamIDs::holdLen, "LENGTH", 92);
        lengthKnob->setBounds (148, 626, 92, 88);
        add<ToggleBox> (a, ParamIDs::holdQuant, "QUANT").setBounds (250, 650, 62, 28);
        add<Knob> (a, ParamIDs::holdMix, "HOLD MIX").setBounds (326, 626, 64, 88);
        add<BandSlider> (a, ParamIDs::holdHP, ParamIDs::holdLP).setBounds (410, 640, 570, 56);

        // --- Séquenceur de pitch ---
        add<PresetBar> (*proc.presets, bbdpresets::Kind::Sequence, false).setBounds (30, 762, 410, 28);
        add<ToggleBox> (a, ParamIDs::seqOn, "SEQ").setBounds (30, 806, 80, 44);
        add<Knob> (a, ParamIDs::seqLength, "STEPS").setBounds (124, 794, 64, 88);
        add<ChoiceBox> (a, ParamIDs::seqStep, "STEP").setBounds (200, 806, 100, 42);
        add<ToggleBox> (a, ParamIDs::seqQuant, "Q SCALE").setBounds (310, 810, 64, 32);
        add<SeqSlotButtons> (*proc.presets).setBounds (386, 806, 84, 64);
        add<Knob> (a, ParamIDs::seqGlide, "GLIDE").setBounds (484, 794, 64, 88);
        add<StepGrid> (a, proc).setBounds (558, 752, 698, 130);

        // --- Presets généraux et A/B ---
        add<PresetBar> (*proc.presets, bbdpresets::Kind::General, true).setBounds (250, 22, 760, 32);

        // --- Output ---
        add<Knob> (a, ParamIDs::mix, "MIX").setBounds (1030, 626, 64, 88);
        add<Knob> (a, ParamIDs::outLevel, "LEVEL").setBounds (1120, 626, 64, 88);

        syncBox->combo.onChange = [this] { updateLfoKnobs(); };
        updateLfoKnobs();

        // Pultec : 0 % s'affiche SOFT, 100 % s'affiche HP, et à 100 % le nom du potard devient HP-6dB
        pultecKnob->slider.textFromValueFunction = [] (double v)
        {
            if (v <= 0.001) return juce::String ("SOFT");
            if (v >= 0.999) return juce::String ("HP");
            return juce::String (juce::roundToInt (v * 100.0)) + " %";
        };
        pultecKnob->slider.onValueChange = [this] { updatePultecLabel(); };
        pultecKnob->slider.updateText();
        updatePultecLabel();

        // Length : avec Quantize, le texte montre la division rythmique réellement utilisée (ex. « 1/8  250 ms »)
        lengthKnob->slider.textFromValueFunction = [this] (double v)
        {
            const float sec = bbdres::holdSecondsFromSlider ((float) v);
            if (holdQuantP->load() <= 0.5f)
                return durationText (sec);
            const float bpm = proc.uiBpm.load();
            const int idx = bbdres::quantizeHoldIndex (sec, bpm);
            if (idx < 0)
                return durationText (sec);
            return juce::String (bbdres::kHoldNames[idx]) + "  " + durationText (bbdres::kHoldBeats[idx] * 60.0f / std::max (bpm, 20.0f));
        };
        lengthKnob->slider.updateText();
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);

        // En-tête
        g.setColour (kAmber);
        g.setFont (font (20.0f, true));
        g.drawText ("DARKSWAN V2", 16, 16, 220, 44, juce::Justification::centredLeft);

        // Panneaux
        panel (g, { 16, 72, 270, 118 }, "INPUT");
        panel (g, { 16, 202, 270, 170 }, "PITCH");
        panel (g, { 298, 72, 654, 524 }, "RESONATOR");
        panel (g, { 964, 72, 300, 186 }, "MOD");
        panel (g, { 964, 270, 300, 118 }, "STEREO");
        panel (g, { 16, 608, 986, 118 }, "HOLD - LOOPER");
        panel (g, { 1014, 608, 250, 118 }, "OUTPUT");
        panel (g, { 16, 738, 1248, 150 }, "SEQUENCEUR DE PITCH");

        // Légende des mémoires de séquence (clic court = rappel, clic long = mémorise)
        g.setColour (kMuted);
        g.setFont (font (9.0f));
        g.drawText ("MEMORY", 386, 790, 84, 12, juce::Justification::centred);

        // Zones réservées pour les fonctions à venir
        dashedBox (g, { 16.0f, 384.0f, 270.0f, 212.0f }, u8 ("RÉSERVÉ - NOUVELLES FONCTIONS"));
        dashedBox (g, { 964.0f, 400.0f, 300.0f, 196.0f }, u8 ("RÉSERVÉ"));

        // Résonateurs (le deuxième est pour plus tard)
        g.setColour (kAmber);
        g.drawRoundedRectangle (310.5f, 496.5f, 55.0f, 27.0f, 3.0f, 1.0f);
        g.setFont (font (11.0f, true));
        g.drawText ("R1", 310, 496, 56, 28, juce::Justification::centred);
        dashedBox (g, { 374.0f, 496.0f, 190.0f, 28.0f }, u8 ("+ RÉSONATEUR - v3"));

        // Séparateurs dans le panneau Resonator : Loop LP et Loop Dyn forment un groupe
        g.setColour (kBorder);
        g.drawLine (556.0f, 128.0f, 556.0f, 222.0f, 1.0f);   // Loop LP + Loop Dyn
        g.drawLine (682.0f, 128.0f, 682.0f, 222.0f, 1.0f);   // Low Cut + Pultec
        g.drawLine (808.0f, 128.0f, 808.0f, 222.0f, 1.0f);   // BBD + Noise
    }

private:
    template <typename T, typename... Args>
    T& add (Args&&... args)
    {
        auto p = std::make_unique<T> (std::forward<Args> (args)...);
        T& ref = *p;
        addAndMakeVisible (ref);
        owned.push_back (std::move (p));
        return ref;
    }

    void updateLfoKnobs()
    {
        const bool freeRunning = syncBox->combo.getSelectedItemIndex() <= 0;
        rateKnob->setVisible (freeRunning);
        phaseKnob->setVisible (! freeRunning);
    }

    void updatePultecLabel()
    {
        pultecKnob->name.setText (pultecKnob->slider.getValue() >= 0.999 ? "HP-6dB" : "PULTEC", juce::dontSendNotification);
    }

    // Le texte de Length dépend du tempo de l'hôte et de Quantize : on le rafraîchit quand ils changent
    void timerCallback() override
    {
        const float bpm = proc.uiBpm.load();
        const bool quant = holdQuantP->load() > 0.5f;
        if (quant != lastQuant || std::abs (bpm - lastBpm) > 0.05f)
        {
            lastQuant = quant;
            lastBpm = bpm;
            lengthKnob->slider.updateText();
        }
    }

    BBDResAudioProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    ChoiceBox* syncBox = nullptr;
    Knob* rateKnob = nullptr;
    Knob* phaseKnob = nullptr;
    Knob* lengthKnob = nullptr;
    Knob* pultecKnob = nullptr;
    std::atomic<float>* holdQuantP = nullptr;
    float lastBpm = 120.0f;
    bool lastQuant = false;
};
} // namespace

BBDResAudioProcessorEditor::BBDResAudioProcessorEditor (BBDResAudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);
    content = std::make_unique<Content> (proc);
    addAndMakeVisible (*content);

    constrainer.setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    constrainer.setSizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (kDesignW, kDesignH);
}

BBDResAudioProcessorEditor::~BBDResAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void BBDResAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bbdui::kBg);
}

void BBDResAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, kDesignW, kDesignH);
}
