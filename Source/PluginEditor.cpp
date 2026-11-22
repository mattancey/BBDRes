#include "PluginEditor.h"
#include "Params.h"
#include "UI/Widgets.h"
#include "UI/Views.h"
#include "UI/StepGrid.h"
#include "UI/PresetBar.h"
#include "UI/SeqSlots.h"
#include "UI/CqtView.h"
#include "UI/QuoteView.h"
#include "UI/FreezeSlots.h"

namespace
{
using namespace bbdui;

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

        // --- Analyseur CQT de la sortie wet ---
        add<CqtView> (proc).setBounds (22, 390, 258, 200);

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
        add<ResponseView> (a, proc).setBounds (310, 272, 630, 150);

        // --- Résonateurs multiples : 4 bandeaux (allumé, solo, intervalle, niveau, balance) ---
        for (int v = 0; v < bbdres::kNumVoices; ++v)
        {
            const int x0 = 314 + v * 158;
            auto& on = add<ToggleBox> (a, ParamIDs::kVoiceOnIDs[v], "R" + juce::String (v + 1));
            on.setBounds (x0, 438, 46, 26);
            on.button.setColour (juce::TextButton::buttonOnColourId, kVoiceColours[v]);
            auto& solo = add<ToggleBox> (a, ParamIDs::kVoiceSoloIDs[v], "S");
            solo.setBounds (x0 + 50, 438, 30, 26);
            solo.button.setColour (juce::TextButton::buttonOnColourId, kText);
            add<Knob> (a, ParamIDs::kVoiceIntIDs[v], "INT", 48).setBounds (x0, 470, 48, 88);
            add<Knob> (a, ParamIDs::kVoiceLvlIDs[v], "LVL", 48).setBounds (x0 + 50, 470, 48, 88);
            add<Knob> (a, ParamIDs::kVoicePanIDs[v], "PAN", 48).setBounds (x0 + 100, 470, 48, 88);
        }

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
        add<ToggleBox> (a, ParamIDs::freeze, "FREEZE").setBounds (30, 636, 150, 44);
        add<FreezeSlotButtons> (proc).setBounds (30, 688, 150, 30);
        lengthKnob = &add<Knob> (a, ParamIDs::holdLen, "LENGTH", 92);
        lengthKnob->setBounds (190, 626, 92, 88);
        add<ToggleBox> (a, ParamIDs::holdQuant, "QUANT").setBounds (292, 650, 62, 28);
        add<Knob> (a, ParamIDs::holdMix, "HOLD MIX").setBounds (366, 626, 64, 88);
        add<Knob> (a, ParamIDs::holdPhase, "PHASE").setBounds (440, 626, 64, 88);
        add<BandSlider> (a, ParamIDs::holdHP, ParamIDs::holdLP).setBounds (520, 640, 470, 56);

        // --- Séquenceur de pitch ---
        add<PresetBar> (*proc.presets, bbdpresets::Kind::Sequence, false).setBounds (30, 762, 410, 28);
        add<ToggleBox> (a, ParamIDs::seqOn, "SEQ").setBounds (30, 806, 80, 44);
        add<Knob> (a, ParamIDs::seqLength, "STEPS").setBounds (124, 794, 64, 88);
        add<ChoiceBox> (a, ParamIDs::seqStep, "STEP").setBounds (200, 796, 100, 42);
        add<ToggleBox> (a, ParamIDs::seqQuant, "Q SCALE").setBounds (308, 800, 56, 28);
        add<SeqSlotButtons> (*proc.presets).setBounds (200, 850, 164, 28);
        add<Knob> (a, ParamIDs::seqGlide, "GLIDE").setBounds (382, 794, 64, 88);
        add<StepGrid> (a, proc).setBounds (456, 752, 800, 130);

        // --- Presets généraux et A/B ---
        add<PresetBar> (*proc.presets, bbdpresets::Kind::General, true).setBounds (250, 22, 760, 32);

        // --- Citations ---
        add<QuoteView> (a).setBounds (964, 400, 300, 196);

        // --- Output ---
        add<Knob> (a, ParamIDs::mix, "MIX").setBounds (1030, 626, 64, 88);
        add<Knob> (a, ParamIDs::outLevel, "WET LEVEL").setBounds (1120, 626, 64, 88);

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
        panel (g, { 16, 384, 270, 212 }, "");   // pas de titre : l'analyseur occupe toute la fenêtre
        panel (g, { 298, 72, 654, 524 }, "RESONATOR");
        panel (g, { 964, 72, 300, 186 }, "MOD");
        panel (g, { 964, 270, 300, 118 }, "STEREO");
        panel (g, { 16, 608, 986, 118 }, "HOLD - LOOPER");
        panel (g, { 1014, 608, 250, 118 }, "OUTPUT");
        panel (g, { 16, 738, 1248, 150 }, "SEQUENCEUR DE PITCH");

        // Légende des mémoires de séquence (clic court = rappel, clic long = mémorise)
        g.setColour (kMuted);
        g.setFont (font (9.0f));
        g.drawText ("LIVE MEMORIES", 200, 838, 164, 11, juce::Justification::centred);

        // Zones réservées pour les fonctions à venir
        panel (g, { 964, 400, 300, 196 }, "");   // les citations

        // Bandeaux des quatre résonateurs, chacun dans sa couleur (la même que sa courbe)
        for (int v = 0; v < bbdres::kNumVoices; ++v)
        {
            const float x0 = (float) (310 + v * 158);
            g.setColour (kDark);
            g.fillRoundedRectangle (x0, 430.0f, 154.0f, 134.0f, 5.0f);
            g.setColour (kVoiceColours[v]);
            g.fillRect (x0 + 4.0f, 430.0f, 146.0f, 2.0f);
        }

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
