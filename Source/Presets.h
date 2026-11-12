#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "FactoryPresets.h"

// Gestionnaire de presets de DarkSwan.
//  - Preset GÉNÉRAL : tous les réglages, y compris la séquence en cours et les 4 mémoires de séquence.
//    Ce qui n'est pas dans le preset revient à sa valeur par défaut.
//  - Preset de SÉQUENCE : seulement la longueur, la durée d'un pas, le glide, Q SCALE et les 16 pas.
//  - Mémoires de séquence (4) : stockées dans l'état du plug-in (donc dans le projet) et dans les presets généraux.
//  - Fondu : un preset général ou un changement A/B peut s'installer progressivement (réglage Preset Fade) ;
//    les réglages continus glissent (avec un lissage doux), les choix et interrupteurs basculent à mi-fondu.
// Les presets de l'utilisateur sont des fichiers XML dans ~/Library/Application Support/DarkSwan/.
namespace bbdpresets
{
enum class Kind { General, Sequence };
constexpr int kNumSeqSlots = 4;

class PresetManager : private juce::Timer
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& a) : apvts (a) {}
    ~PresetManager() override { cancelMorph(); }

    juce::AudioProcessorValueTreeState& state() { return apvts; }

    // ---- listes ----
    juce::StringArray factoryNames (Kind k) const
    {
        juce::StringArray names;
        for (const auto& p : factory (k))
            names.add (p.name);
        return names;
    }

    juce::StringArray userNames (Kind k) const
    {
        juce::StringArray names;
        for (const auto& f : folder (k).findChildFiles (juce::File::findFiles, false, "*.xml"))
            names.add (f.getFileNameWithoutExtension());
        names.sort (true);
        return names;
    }

    juce::String currentName (Kind k) const
    {
        return apvts.state.getProperty (nameProperty (k), "Init").toString();
    }

    // ---- chargement et enregistrement ----
    bool loadFactory (Kind k, int index)
    {
        const auto& list = factory (k);
        if (index < 0 || index >= (int) list.size()) return false;
        std::map<juce::String, float> values;
        for (const auto& pv : list[(size_t) index].values)
            values[pv.id] = pv.value;
        if (k == Kind::General) clearSlots();
        applyValues (k, values, k == Kind::General ? fadeSeconds() : 0.0);
        apvts.state.setProperty (nameProperty (k), list[(size_t) index].name, nullptr);
        return true;
    }

    bool loadUser (Kind k, const juce::String& name)
    {
        const auto xml = juce::XmlDocument::parse (fileFor (k, name));
        if (xml == nullptr || ! xml->hasTagName ("DarkSwanPreset")) return false;
        std::map<juce::String, float> values;
        for (const auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        if (k == Kind::General)
        {
            clearSlots();
            for (const auto* e : xml->getChildWithTagNameIterator ("SLOT"))
                setSlotString (e->getIntAttribute ("index"), e->getStringAttribute ("data"));
        }
        applyValues (k, values, k == Kind::General ? fadeSeconds() : 0.0);
        apvts.state.setProperty (nameProperty (k), name, nullptr);
        return true;
    }

    bool saveUser (Kind k, const juce::String& name)
    {
        const auto legal = juce::File::createLegalFileName (name.trim());
        if (legal.isEmpty()) return false;
        const auto dir = folder (k);
        if (! dir.createDirectory().wasOk()) return false;

        juce::XmlElement xml ("DarkSwanPreset");
        xml.setAttribute ("type", k == Kind::General ? "general" : "sequence");
        xml.setAttribute ("name", legal);
        for (const auto& kv : snapshot (k))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", kv.first);
            e->setAttribute ("value", (double) kv.second);
        }
        if (k == Kind::General)
            for (int i = 0; i < kNumSeqSlots; ++i)
                if (slotUsed (i))
                {
                    auto* e = xml.createNewChildElement ("SLOT");
                    e->setAttribute ("index", i);
                    e->setAttribute ("data", slotString (i));
                }
        if (! dir.getChildFile (legal + ".xml").replaceWithText (xml.toString())) return false;
        apvts.state.setProperty (nameProperty (k), legal, nullptr);
        return true;
    }

    bool deleteUser (Kind k, const juce::String& name)
    {
        return fileFor (k, name).deleteFile();
    }

    // ---- mémoires de séquence (stockées dans l'état du plug-in) ----
    bool slotUsed (int i) const { return slotString (i).isNotEmpty(); }

    void saveSeqSlot (int i)
    {
        if (i >= 0 && i < kNumSeqSlots) setSlotString (i, serialize (snapshot (Kind::Sequence)));
    }

    bool recallSeqSlot (int i)
    {
        if (i < 0 || i >= kNumSeqSlots || ! slotUsed (i)) return false;
        applyValues (Kind::Sequence, deserialize (slotString (i)), 0.0);
        return true;
    }

    // ---- A/B (états complets : réglages et mémoires, le temps de la session) ----
    int activeSlot() const { return active; }

    void switchSlot (int target)
    {
        if (target == active || target < 0 || target > 1) return;
        ab[active] = capture();
        if (ab[target].valid)
        {
            restoreSlots (ab[target].slots);
            applyValues (Kind::General, ab[target].values, fadeSeconds());
        }
        else
            ab[target] = ab[active];   // première visite : on part du même son
        active = target;
    }

    void copyToOtherSlot() { ab[1 - active] = capture(); }

private:
    struct AB
    {
        std::map<juce::String, float> values;
        juce::StringArray slots;
        bool valid = false;
    };

    struct Ramp
    {
        juce::RangedAudioParameter* param;
        float from, to;
        bool discrete, done;
    };

    static bool isSequenceParam (const juce::String& id)
    {
        return id == ParamIDs::seqLength || id == ParamIDs::seqStep || id == ParamIDs::seqQuant
            || id == ParamIDs::seqGlide || id.startsWith ("step");
    }

    // réglages globaux : jamais touchés par un preset ni par le A/B
    static bool isGlobalParam (const juce::String& id) { return id == ParamIDs::presetFade; }

    static const std::vector<FactoryPreset>& factory (Kind k)
    {
        return k == Kind::General ? generalFactory() : sequenceFactory();
    }

    static juce::String nameProperty (Kind k) { return k == Kind::General ? "presetName" : "seqPresetName"; }

    static juce::File folder (Kind k)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Application Support").getChildFile ("DarkSwan")
            .getChildFile (k == Kind::General ? "Presets" : "Sequences");
    }

    static juce::File fileFor (Kind k, const juce::String& name)
    {
        return folder (k).getChildFile (juce::File::createLegalFileName (name) + ".xml");
    }

    static juce::String serialize (const std::map<juce::String, float>& m)
    {
        juce::String s;
        for (const auto& kv : m)
            s << kv.first << "=" << juce::String (kv.second, 3) << ";";
        return s;
    }

    static std::map<juce::String, float> deserialize (const juce::String& s)
    {
        std::map<juce::String, float> m;
        juce::StringArray parts;
        parts.addTokens (s, ";", "");
        for (const auto& part : parts)
        {
            const int eq = part.indexOfChar ('=');
            if (eq > 0) m[part.substring (0, eq)] = part.substring (eq + 1).getFloatValue();
        }
        return m;
    }

    double fadeSeconds() const
    {
        if (auto* p = apvts.getRawParameterValue (ParamIDs::presetFade)) return (double) p->load();
        return 0.0;
    }

    // ---- mémoires de séquence : propriétés s0..s3 du nœud SEQSLOTS de l'état ----
    juce::String slotString (int i) const
    {
        const auto node = apvts.state.getChildWithName ("SEQSLOTS");
        return node.isValid() ? node.getProperty ("s" + juce::String (i)).toString() : juce::String();
    }
    void setSlotString (int i, const juce::String& s)
    {
        if (i < 0 || i >= kNumSeqSlots) return;
        apvts.state.getOrCreateChildWithName ("SEQSLOTS", nullptr).setProperty ("s" + juce::String (i), s, nullptr);
    }
    void clearSlots()
    {
        const auto node = apvts.state.getChildWithName ("SEQSLOTS");
        if (node.isValid()) apvts.state.removeChild (node, nullptr);
    }
    void restoreSlots (const juce::StringArray& slots)
    {
        clearSlots();
        for (int i = 0; i < slots.size() && i < kNumSeqSlots; ++i)
            if (slots[i].isNotEmpty()) setSlotString (i, slots[i]);
    }

    AB capture() const
    {
        AB s;
        s.values = snapshot (Kind::General);
        for (int i = 0; i < kNumSeqSlots; ++i) s.slots.add (slotString (i));
        s.valid = true;
        return s;
    }

    // valeurs réelles de tous les réglages concernés
    std::map<juce::String, float> snapshot (Kind k) const
    {
        std::map<juce::String, float> values;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (isGlobalParam (rp->paramID)) continue;
                if (k == Kind::Sequence && ! isSequenceParam (rp->paramID)) continue;
                values[rp->paramID] = rp->convertFrom0to1 (rp->getValue());
            }
        return values;
    }

    // Applique les valeurs ; ce qui manque revient à la valeur par défaut. À appeler depuis le fil de l'interface.
    // fadeSeconds > 0 : fondu. Les réglages continus glissent (courbe douce), les choix et interrupteurs basculent à mi-fondu.
    void applyValues (Kind k, const std::map<juce::String, float>& values, double fade)
    {
        cancelMorph();   // un fondu en cours s'arrête là où il est : le suivant part de cette position
        std::vector<Ramp> ramps;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (isGlobalParam (rp->paramID)) continue;
                if (k == Kind::Sequence && ! isSequenceParam (rp->paramID)) continue;
                const auto it = values.find (rp->paramID);
                const float real = it != values.end() ? it->second : rp->convertFrom0to1 (rp->getDefaultValue());
                const bool discrete = dynamic_cast<juce::AudioParameterFloat*> (rp) == nullptr;
                ramps.push_back ({ rp, rp->getValue(), rp->convertTo0to1 (real), discrete, false });
            }

        if (fade < 0.02)
        {
            for (auto& r : ramps)
            {
                r.param->beginChangeGesture();
                r.param->setValueNotifyingHost (r.to);
                r.param->endChangeGesture();
            }
            return;
        }
        morph = std::move (ramps);
        for (auto& r : morph) r.param->beginChangeGesture();
        morphStart = juce::Time::getMillisecondCounterHiRes();
        morphSeconds = fade;
        startTimerHz (60);
    }

    void cancelMorph()
    {
        stopTimer();
        for (auto& r : morph) r.param->endChangeGesture();
        morph.clear();
    }

    void timerCallback() override
    {
        const double t = (juce::Time::getMillisecondCounterHiRes() - morphStart) / 1000.0 / morphSeconds;
        if (t >= 1.0)
        {
            for (auto& r : morph)
            {
                r.param->setValueNotifyingHost (r.to);
                r.param->endChangeGesture();
            }
            morph.clear();
            stopTimer();
            return;
        }
        const float s = (float) (t * t * (3.0 - 2.0 * t));   // départ et arrivée en douceur
        for (auto& r : morph)
        {
            if (r.discrete)
            {
                if (! r.done && t >= 0.5) { r.param->setValueNotifyingHost (r.to); r.done = true; }
            }
            else
                r.param->setValueNotifyingHost (r.from + (r.to - r.from) * s);
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    AB ab[2];
    int active = 0;
    std::vector<Ramp> morph;
    double morphStart = 0.0, morphSeconds = 0.0;
};
} // namespace bbdpresets
