#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Palette et habillage de BBDRes (style « panneau matériel » de la maquette A)
namespace bbdui
{
inline const juce::Colour kBg     { 0xff17191c };
inline const juce::Colour kPanel  { 0xff1e2125 };
inline const juce::Colour kBorder { 0xff363a40 };
inline const juce::Colour kLine   { 0xff50555c };
inline const juce::Colour kDark   { 0xff14161a };
inline const juce::Colour kAmber  { 0xfff2a33a };
inline const juce::Colour kText   { 0xffd9d4c7 };
inline const juce::Colour kMuted  { 0xff9aa0a6 };

inline juce::Font font (float height, bool bold = false)
{
    juce::Font f { juce::FontOptions (height) };
    return bold ? f.boldened() : f;
}

// Texte accentué : toujours passer par fromUTF8
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

class BBDLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BBDLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, kBg);
        setColour (juce::Label::textColourId, kMuted);
        setColour (juce::Slider::textBoxTextColourId, kText);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, kDark);
        setColour (juce::ComboBox::textColourId, kText);
        setColour (juce::ComboBox::outlineColourId, kLine);
        setColour (juce::ComboBox::arrowColourId, kMuted);
        setColour (juce::PopupMenu::backgroundColourId, kPanel);
        setColour (juce::PopupMenu::textColourId, kText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAmber);
        setColour (juce::PopupMenu::highlightedTextColourId, kBg);
        setColour (juce::TextButton::buttonColourId, kDark);
        setColour (juce::TextButton::buttonOnColourId, kAmber);
        setColour (juce::TextButton::textColourOffId, kText);
        setColour (juce::TextButton::textColourOnId, kBg);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.0f);
        const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // corps du potard
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff41464d), centre.x, centre.y - radius * 0.3f,
                                                 juce::Colour (0xff23262b), centre.x, centre.y + radius, true));
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (kLine);
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // arc de valeur, du départ jusqu'à la position
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius + 3.0f, radius + 3.0f, 0.0f, startAngle, angle, true);
        g.setColour (kAmber.withAlpha (0.55f));
        g.strokePath (arc, juce::PathStrokeType (2.0f));

        // repère
        juce::Path tick;
        tick.addRoundedRectangle (-1.75f, -radius + 3.0f, 3.5f, radius * 0.38f, 1.5f);
        g.setColour (kAmber);
        g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    juce::Font getLabelFont (juce::Label&) override { return font (10.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f, true); }
};
} // namespace bbdui
