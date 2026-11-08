#include "PluginEditor.h"
#include "Params.h"
#include "UI/Widgets.h"

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

class Content : public juce::Component
{
public:
    explicit Content (juce::AudioProcessorValueTreeState& a)
    {
        // --- Input ---
        add<Knob> (a, ParamIDs::drive, "DRIVE").setBounds (28, 98, 64, 88);
        add<ChoiceBox> (a, ParamIDs::driveCurve, "CURVE").setBounds (108, 116, 76, 42);
        add<Knob> (a, ParamIDs::inputLP, "IN LP 12").setBounds (196, 98, 64, 88);

        // --- Pitch ---
        add<ChoiceBox> (a, ParamIDs::root, "ROOT").setBounds (28, 232, 70, 42);
        add<ChoiceBox> (a, ParamIDs::scale, "SCALE").setBounds (104, 232, 100, 42);
        add<ToggleBox> (a, ParamIDs::quantize, "QUANT").setBounds (212, 232, 62, 28);

        // --- Resonator ---
        add<Knob> (a, ParamIDs::tune, "TUNE").setBounds (310, 104, 120, 150);
        add<Knob> (a, ParamIDs::fine, "FINE").setBounds (440, 138, 64, 88);
        add<Knob> (a, ParamIDs::feedback, "FEEDBACK").setBounds (516, 130, 64, 88);
        add<Knob> (a, ParamIDs::loopLP, "LOOP LP 6").setBounds (604, 130, 64, 88);
        add<Knob> (a, ParamIDs::loopDyn, "LOOP DYN").setBounds (672, 130, 64, 88);
        add<Knob> (a, ParamIDs::bbd, "BBD").setBounds (760, 130, 64, 88);
        add<Knob> (a, ParamIDs::noise, "NOISE").setBounds (832, 130, 64, 88);

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
        add<Knob> (a, ParamIDs::holdLen, "LENGTH").setBounds (160, 626, 64, 88);
        add<ToggleBox> (a, ParamIDs::holdQuant, "QUANT").setBounds (240, 650, 64, 28);
        add<Knob> (a, ParamIDs::holdMix, "HOLD MIX").setBounds (320, 626, 64, 88);
        add<BandSlider> (a, ParamIDs::holdHP, ParamIDs::holdLP).setBounds (410, 640, 570, 56);

        // --- Output ---
        add<Knob> (a, ParamIDs::mix, "MIX").setBounds (1030, 626, 64, 88);
        add<Knob> (a, ParamIDs::outLevel, "LEVEL").setBounds (1120, 626, 64, 88);

        syncBox->combo.onChange = [this] { updateLfoKnobs(); };
        updateLfoKnobs();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);

        // En-tête
        g.setColour (kAmber);
        g.setFont (font (20.0f, true));
        g.drawText ("BBDRES V2", 16, 16, 220, 44, juce::Justification::centredLeft);
        dashedBox (g, { 250.0f, 22.0f, 760.0f, 32.0f }, u8 ("PRESETS ET A/B : BIENTÔT"));

        // Panneaux
        panel (g, { 16, 72, 270, 118 }, "INPUT");
        panel (g, { 16, 202, 270, 170 }, "PITCH");
        panel (g, { 298, 72, 654, 524 }, "RESONATOR");
        panel (g, { 964, 72, 300, 186 }, "MOD");
        panel (g, { 964, 270, 300, 118 }, "STEREO");
        panel (g, { 16, 608, 986, 118 }, "HOLD - LOOPER");
        panel (g, { 1014, 608, 250, 118 }, "OUTPUT");

        // Zones réservées pour les fonctions à venir
        dashedBox (g, { 16.0f, 384.0f, 270.0f, 212.0f }, u8 ("RÉSERVÉ - NOUVELLES FONCTIONS"));
        dashedBox (g, { 964.0f, 400.0f, 300.0f, 196.0f }, u8 ("RÉSERVÉ"));
        dashedBox (g, { 16.0f, 738.0f, 1248.0f, 26.0f }, u8 ("SÉQUENCEUR DE PITCH : tiroir replié (v3)"));

        // Emplacements des widgets de l'étape 2
        dashedBox (g, { 28.0f, 284.0f, 246.0f, 60.0f }, "CLAVIER DE GAMME - etape 2");
        dashedBox (g, { 310.0f, 272.0f, 630.0f, 210.0f }, u8 ("COURBE DE RÉPONSE - etape 2"));

        // Résonateurs (le deuxième est pour plus tard)
        g.setColour (kAmber);
        g.drawRoundedRectangle (310.5f, 496.5f, 55.0f, 27.0f, 3.0f, 1.0f);
        g.setFont (font (11.0f, true));
        g.drawText ("R1", 310, 496, 56, 28, juce::Justification::centred);
        dashedBox (g, { 374.0f, 496.0f, 190.0f, 28.0f }, u8 ("+ RÉSONATEUR - v3"));

        // Séparateurs dans le panneau Resonator : Loop LP et Loop Dyn forment un groupe
        g.setColour (kBorder);
        g.drawLine (594.0f, 128.0f, 594.0f, 222.0f, 1.0f);
        g.drawLine (748.0f, 128.0f, 748.0f, 222.0f, 1.0f);
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

    std::vector<std::unique_ptr<juce::Component>> owned;
    ChoiceBox* syncBox = nullptr;
    Knob* rateKnob = nullptr;
    Knob* phaseKnob = nullptr;
};
} // namespace

BBDResAudioProcessorEditor::BBDResAudioProcessorEditor (BBDResAudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);
    content = std::make_unique<Content> (proc.apvts);
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
