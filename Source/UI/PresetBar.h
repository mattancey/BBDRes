#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "../Presets.h"
#include "../Params.h"

namespace bbdui
{
// Barre de presets : précédent, liste (usine puis utilisateur), suivant, SAVE, DEL, et en option A / B / COPY.
class PresetBar : public juce::Component
{
public:
    PresetBar (bbdpresets::PresetManager& m, bbdpresets::Kind k, bool withAB) : manager (m), kind (k), showAB (withAB)
    {
        prevButton.setButtonText ("<");
        nextButton.setButtonText (">");
        saveButton.setButtonText ("SAVE");
        deleteButton.setButtonText ("DEL");
        addAndMakeVisible (prevButton);
        addAndMakeVisible (nextButton);
        addAndMakeVisible (saveButton);
        addAndMakeVisible (deleteButton);
        addAndMakeVisible (combo);

        prevButton.onClick = [this] { stepPreset (-1); };
        nextButton.onClick = [this] { stepPreset (1); };
        saveButton.onClick = [this] { promptSave(); };
        deleteButton.onClick = [this]
        {
            if (userNames.contains (manager.currentName (kind)))
            {
                manager.deleteUser (kind, manager.currentName (kind));
                refresh();
            }
        };
        combo.onChange = [this]
        {
            const int id = combo.getSelectedId();
            if (id >= 1000)       manager.loadUser (kind, userNames[id - 1000]);
            else if (id >= 1)     manager.loadFactory (kind, id - 1);
            else                  return;
            combo.setText (manager.currentName (kind), juce::dontSendNotification);
        };

        if (showAB)
        {
            aButton.setButtonText ("A");
            bButton.setButtonText ("B");
            copyButton.setButtonText ("COPY");
            for (auto* b : { &aButton, &bButton })
            {
                b->setClickingTogglesState (true);
                b->setRadioGroupId (4242);
                addAndMakeVisible (*b);
            }
            addAndMakeVisible (copyButton);

            // Fondu entre presets généraux et entre A et B : de 0 à 5 s (réglage global)
            fadeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            fadeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            fadeSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            addAndMakeVisible (fadeSlider);
            fadeLabel.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (fadeLabel);
            fadeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (manager.state(), ParamIDs::presetFade, fadeSlider);
            fadeSlider.onValueChange = [this] { updateFadeLabel(); };
            updateFadeLabel();
            aButton.onClick = [this] { if (aButton.getToggleState()) manager.switchSlot (0); };
            bButton.onClick = [this] { if (bButton.getToggleState()) manager.switchSlot (1); };
            copyButton.onClick = [this] { manager.copyToOtherSlot(); };
        }
        refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (showAB)
        {
            fadeLabel.setBounds (r.removeFromRight (74));
            fadeSlider.setBounds (r.removeFromRight (30)); r.removeFromRight (8);
            copyButton.setBounds (r.removeFromRight (62)); r.removeFromRight (6);
            bButton.setBounds (r.removeFromRight (36));    r.removeFromRight (4);
            aButton.setBounds (r.removeFromRight (36));    r.removeFromRight (10);
        }
        deleteButton.setBounds (r.removeFromRight (52));   r.removeFromRight (6);
        saveButton.setBounds (r.removeFromRight (60));     r.removeFromRight (10);
        prevButton.setBounds (r.removeFromLeft (28));      r.removeFromLeft (4);
        nextButton.setBounds (r.removeFromRight (28));     r.removeFromRight (4);
        combo.setBounds (r);
    }

private:
    void updateFadeLabel()
    {
        fadeLabel.setText ("FADE " + juce::String (fadeSlider.getValue(), 1) + " s", juce::dontSendNotification);
    }

    // reconstruit la liste : presets d'usine (1..), puis presets de l'utilisateur (1000..)
    void refresh()
    {
        combo.clear (juce::dontSendNotification);
        factoryNames = manager.factoryNames (kind);
        userNames = manager.userNames (kind);
        combo.addSectionHeading ("FACTORY");
        for (int i = 0; i < factoryNames.size(); ++i)
            combo.addItem (factoryNames[i], i + 1);
        if (userNames.size() > 0)
        {
            combo.addSectionHeading ("USER");
            for (int i = 0; i < userNames.size(); ++i)
                combo.addItem (userNames[i], 1000 + i);
        }
        combo.setText (manager.currentName (kind), juce::dontSendNotification);
        deleteButton.setEnabled (userNames.contains (manager.currentName (kind)));
        if (showAB)
        {
            aButton.setToggleState (manager.activeSlot() == 0, juce::dontSendNotification);
            bButton.setToggleState (manager.activeSlot() == 1, juce::dontSendNotification);
        }
    }

    // passe au preset suivant ou précédent de la liste complète
    void stepPreset (int direction)
    {
        juce::StringArray all (factoryNames);
        all.addArray (userNames);
        if (all.isEmpty()) return;
        const int current = all.indexOf (manager.currentName (kind));
        const int next = (current < 0 ? (direction > 0 ? 0 : all.size() - 1) : (current + direction + all.size()) % all.size());
        if (next < factoryNames.size()) manager.loadFactory (kind, next);
        else                            manager.loadUser (kind, all[next]);
        refresh();
    }

    void promptSave()
    {
        auto* window = new juce::AlertWindow ("SAVE", "Name", juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", manager.currentName (kind));
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBar> safe (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            const auto name = window->getTextEditorContents ("name").trim();
            if (name.isNotEmpty() && safe->manager.saveUser (safe->kind, name))
                safe->refresh();
        }), true);
    }

    bbdpresets::PresetManager& manager;
    bbdpresets::Kind kind;
    bool showAB;
    juce::StringArray factoryNames, userNames;
    juce::ComboBox combo;
    juce::TextButton prevButton, nextButton, saveButton, deleteButton, aButton, bButton, copyButton;
    juce::Slider fadeSlider;
    juce::Label fadeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fadeAttachment;
};
} // namespace bbdui
