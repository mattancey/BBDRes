#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BBDLookAndFeel.h"
#include "../Quotes.h"

namespace bbdui
{
// Une citation au hasard, en Helvetica Light, qui change toutes les 10 minutes (un clic passe à la suivante).
//  - Ordre sans répétition : chaque citation passe une fois avant que la première ne revienne.
//  - La citation en cours et son horaire sont gardés dans l'état du plug-in : fermer et rouvrir la fenêtre, ou le
//    projet, ne la change pas avant l'échéance.
//  - Mise en page : le texte est centré dans la fenêtre. On cherche la plus grande taille (de 20 à 10,5 pt) qui tient ;
//    si la citation compte plusieurs phrases, on passe à la ligne après chaque point (tant que la taille reste lisible,
//    sinon le texte coule normalement). Les lignes sont ensuite équilibrées (on rétrécit la colonne tant que le nombre de
//    lignes ne change pas), ce qui évite les lignes orphelines. Un petit filet ambre encadre les citations courtes.
class QuoteView : public juce::Component, private juce::Timer
{
public:
    explicit QuoteView (juce::AudioProcessorValueTreeState& a) : apvts (a)
    {
        quotes = bbdquotes::all();
        update (false);
        startTimerHz (2);
    }

    void paint (juce::Graphics& g) override
    {
        if (fading)
        {
            const double phase = juce::jlimit (0.0, 1.0, (double) (juce::Time::getMillisecondCounter() - fadeStart) / kFadeMs);
            if (phase < 0.4) drawLaid (g, previous, (float) (1.0 - phase / 0.4));        // l'ancienne s'efface…
            else             drawLaid (g, current, (float) ((phase - 0.4) / 0.6));        // …puis la nouvelle apparaît
        }
        else
            drawLaid (g, current, 1.0f);
    }

    void resized() override
    {
        fading = false;
        if (getWidth() > 0 && ! quotes.empty())
            current = layoutFor (quotes[(size_t) index]);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        update (true);   // un clic passe à la suivante
        beginFade();
    }

private:
    static constexpr juce::int64 kIntervalMs = 10 * 60 * 1000;
    static constexpr juce::uint32 kFadeMs = 2000;

    struct Laid
    {
        juce::TextLayout layout;
        juce::Rectangle<float> area;
        bool ornament = false;
    };

    static juce::Font quoteFont (float size) { return juce::Font (juce::FontOptions ("Helvetica", "Light", size)); }

    juce::TextLayout makeLayout (const juce::String& text, float size, float width) const
    {
        juce::AttributedString a;
        a.setJustification (juce::Justification::centred);
        a.setLineSpacing (0.12f);
        a.append (text, quoteFont (size), kText);
        juce::TextLayout l;
        l.createLayout (a, width);
        return l;
    }

    // Choisit la taille et le découpage, équilibre les lignes, centre le bloc
    Laid layoutFor (const std::string& raw) const
    {
        const auto sentences = bbdquotes::splitSentences (bbdquotes::normalise (raw));
        const float maxW = (float) getWidth() - 40.0f, maxH = (float) getHeight() - 44.0f;
        auto join = [&] (const char* sep)
        {
            std::string s;
            for (size_t i = 0; i < sentences.size(); ++i) { if (i > 0) s += sep; s += sentences[i]; }
            return juce::String::fromUTF8 (s.c_str());
        };
        struct Attempt { juce::String text; float minSize; };
        std::vector<Attempt> attempts;
        if (sentences.size() > 1) attempts.push_back ({ join ("\n"), 12.5f });   // une phrase par ligne, si c'est lisible
        attempts.push_back ({ join (" "), 10.5f });                               // sinon, texte continu

        for (size_t k = 0; k < attempts.size(); ++k)
        {
            const bool lastAttempt = k + 1 == attempts.size();
            for (float size = 20.0f; size >= attempts[k].minSize - 0.01f; size -= 0.5f)
            {
                auto l = makeLayout (attempts[k].text, size, maxW);
                if (l.getHeight() <= maxH || (lastAttempt && size <= attempts[k].minSize + 0.01f))
                    return finish (attempts[k].text, size, maxW, std::move (l));
            }
        }
        return {};
    }

    Laid finish (const juce::String& text, float size, float maxW, juce::TextLayout layout) const
    {
        const int lines = layout.getNumLines();
        float w = maxW;
        for (float tryW = maxW - 8.0f; tryW > maxW * 0.5f; tryW -= 8.0f)   // colonne la plus étroite à nombre de lignes égal
        {
            auto l = makeLayout (text, size, tryW);
            if (l.getNumLines() != lines || l.getHeight() > layout.getHeight() + 0.5f) break;
            layout = std::move (l);
            w = tryW;
        }
        Laid laid;
        const float h = layout.getHeight();
        laid.area = { ((float) getWidth() - w) * 0.5f, ((float) getHeight() - h) * 0.5f - 3.0f, w, h };
        laid.ornament = h <= (float) getHeight() - 74.0f;   // assez de place pour le filet en haut et en bas
        laid.layout = std::move (layout);
        return laid;
    }

    void drawLaid (juce::Graphics& g, const Laid& laid, float alpha) const
    {
        if (alpha <= 0.01f) return;
        g.beginTransparencyLayer (alpha);
        laid.layout.draw (g, laid.area);
        if (laid.ornament)
        {
            g.setColour (kAmber.withAlpha (0.65f));
            const float cx = (float) getWidth() * 0.5f;
            g.fillRect (cx - 14.0f, laid.area.getY() - 16.0f, 28.0f, 1.0f);
            g.fillRect (cx - 14.0f, laid.area.getBottom() + 14.0f, 28.0f, 1.0f);
        }
        g.endTransparencyLayer();
    }

    // Choisit la citation d'après l'état du plug-in : graine, rang dans l'ordre sans répétition, heure du dernier changement
    void update (bool force)
    {
        if (quotes.empty()) return;
        auto& st = apvts.state;
        const auto now = juce::Time::currentTimeMillis();
        const int count = (int) quotes.size();
        unsigned seed = (unsigned) (int) st.getProperty ("quoteSeed", 0);
        int pos = (int) st.getProperty ("quotePos", 0);
        juce::int64 t0 = (juce::int64) st.getProperty ("quoteTime", (juce::int64) 0);
        if (seed == 0u)                              { seed = (unsigned) (now & 0x7fffffff) | 1u; pos = 0; t0 = now; }
        else if (force || now - t0 >= kIntervalMs)   { ++pos; t0 = now; }
        if (pos >= count)                            { seed = (seed * 1664525u + 1013904223u) | 1u; pos = 0; }   // nouveau cycle
        st.setProperty ("quoteSeed", (int) seed, nullptr);
        st.setProperty ("quotePos", pos, nullptr);
        st.setProperty ("quoteTime", t0, nullptr);
        index = bbdquotes::quoteIndexFor (seed, pos, count);
    }

    void beginFade()
    {
        previous = current;
        current = layoutFor (quotes[(size_t) index]);
        fading = true;
        fadeStart = juce::Time::getMillisecondCounter();
        startTimerHz (30);
        repaint();
    }

    void timerCallback() override
    {
        if (fading)
        {
            if (juce::Time::getMillisecondCounter() - fadeStart >= kFadeMs)
            {
                fading = false;
                startTimerHz (2);
            }
            repaint();
            return;
        }
        const int before = index;
        update (false);
        if (index != before) beginFade();
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::string> quotes;
    int index = 0;
    Laid current, previous;
    bool fading = false;
    juce::uint32 fadeStart = 0;
};
} // namespace bbdui
