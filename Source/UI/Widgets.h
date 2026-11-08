#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"

// Briques de l'interface : chaque widget est relié à un paramètre du plug-in.
namespace bbdui
{
// Potard rotatif avec valeur et nom. Double-clic = valeur par défaut (géré par JUCE).
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (slider);
        name.setText (labelText, juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (name);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        name.setBounds (r.removeFromBottom (14));
        slider.setBounds (r);
    }
    juce::Slider slider;
    juce::Label name;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Menu déroulant (paramètre à choix) avec légende
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
            combo.addItemList (cp->choices, 1);
        addAndMakeVisible (combo);
        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, combo);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (14));
        combo.setBounds (r.removeFromTop (28));
    }
    juce::ComboBox combo;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Bouton à bascule (paramètre booléen)
class ToggleBox : public juce::Component
{
public:
    ToggleBox (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& text)
    {
        button.setButtonText (text);
        button.setClickingTogglesState (true);
        addAndMakeVisible (button);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, button);
    }
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Curseur à deux poignées : passe-haut à gauche, passe-bas à droite (un paramètre chacune).
// Les poignées ne se croisent pas. Double-clic = tout ouvrir.
class BandSlider : public juce::Component
{
public:
    BandSlider (juce::AudioProcessorValueTreeState& apvts, const juce::String& hpID, const juce::String& lpID)
        : hp (*apvts.getParameter (hpID)), lp (*apvts.getParameter (lpID)),
          hpAttach (hp, [this] (float) { repaint(); }),
          lpAttach (lp, [this] (float) { repaint(); })
    {
        hpAttach.sendInitialUpdate();
        lpAttach.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        auto top = area.removeFromTop (14);
        g.setFont (font (9.0f));
        g.setColour (kMuted);
        g.drawText ("HP 12 - " + hp.getText (hp.getValue(), 0), top, juce::Justification::centredLeft);
        g.drawText ("FILTRE DU LOOPER", top, juce::Justification::centred);
        g.drawText ("LP 12 - " + lp.getText (lp.getValue(), 0), top, juce::Justification::centredRight);

        const float cy = (float) area.getCentreY();
        const float xh = handleX (hp.getValue()), xl = handleX (lp.getValue());
        g.setColour (kLine);
        g.drawLine (trackL(), cy, trackR(), cy, 3.0f);
        g.setColour (kAmber);
        g.drawLine (xh, cy, xl, cy, 5.0f);
        g.fillEllipse (xh - 9.0f, cy - 9.0f, 18.0f, 18.0f);
        g.fillEllipse (xl - 9.0f, cy - 9.0f, 18.0f, 18.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float xh = handleX (hp.getValue()), xl = handleX (lp.getValue());
        const float dh = std::abs ((float) e.x - xh), dl = std::abs ((float) e.x - xl);
        active = (dh < dl) ? 0 : (dl < dh ? 1 : ((float) e.x < xh ? 0 : 1));   // poignées jointes : selon le côté du clic
        (active == 0 ? hpAttach : lpAttach).beginGesture();
        dragTo (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { dragTo (e); }
    void mouseUp (const juce::MouseEvent&) override { (active == 0 ? hpAttach : lpAttach).endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        hpAttach.setValueAsCompleteGesture (hp.convertFrom0to1 (hp.getDefaultValue()));
        lpAttach.setValueAsCompleteGesture (lp.convertFrom0to1 (lp.getDefaultValue()));
    }

private:
    float trackL() const { return 12.0f; }
    float trackR() const { return (float) getWidth() - 12.0f; }
    float handleX (float norm) const { return trackL() + norm * (trackR() - trackL()); }

    void dragTo (const juce::MouseEvent& e)
    {
        float n = juce::jlimit (0.0f, 1.0f, ((float) e.x - trackL()) / (trackR() - trackL()));
        if (active == 0) n = std::min (n, lp.getValue());
        else             n = std::max (n, hp.getValue());
        auto& att = (active == 0) ? hpAttach : lpAttach;
        auto& prm = (active == 0) ? hp : lp;
        att.setValueAsPartOfGesture (prm.convertFrom0to1 (n));
    }

    juce::RangedAudioParameter& hp;
    juce::RangedAudioParameter& lp;
    juce::ParameterAttachment hpAttach, lpAttach;
    int active = 0;
};
} // namespace bbdui
