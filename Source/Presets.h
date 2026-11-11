#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "FactoryPresets.h"

// Gestionnaire de presets de DarkSwan.
//  - Preset GÉNÉRAL : tous les réglages, y compris la séquence en cours. Ce qui n'est pas dans le
//    preset revient à sa valeur par défaut.
//  - Preset de SÉQUENCE : seulement la longueur, la durée d'un pas, le glide, Q SCALE et les 16 pas
//    (hauteur et activation). Le reste du son n'est pas touché, ni l'interrupteur SEQ.
// Les presets de l'utilisateur sont des fichiers XML dans ~/Library/Application Support/DarkSwan/.
// Le A/B garde deux états complets pendant la session (comparaison rapide).
namespace bbdpresets
{
enum class Kind { General, Sequence };

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& a) : apvts (a) {}

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
        apply (k, values);
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
        apply (k, values);
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
        if (! dir.getChildFile (legal + ".xml").replaceWithText (xml.toString())) return false;
        apvts.state.setProperty (nameProperty (k), legal, nullptr);
        return true;
    }

    bool deleteUser (Kind k, const juce::String& name)
    {
        return fileFor (k, name).deleteFile();
    }

    // ---- A/B (états complets, le temps de la session) ----
    int activeSlot() const { return active; }

    void switchSlot (int target)
    {
        if (target == active || target < 0 || target > 1) return;
        slot[active] = snapshot (Kind::General);
        slotValid[active] = true;
        if (slotValid[target]) apply (Kind::General, slot[target]);
        else { slot[target] = slot[active]; slotValid[target] = true; }   // première visite : on part du même son
        active = target;
    }

    void copyToOtherSlot()
    {
        slot[1 - active] = snapshot (Kind::General);
        slotValid[1 - active] = true;
    }

private:
    static bool isSequenceParam (const juce::String& id)
    {
        return id == ParamIDs::seqLength || id == ParamIDs::seqStep || id == ParamIDs::seqQuant
            || id == ParamIDs::seqGlide || id.startsWith ("step");
    }

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

    // valeurs réelles de tous les réglages concernés
    std::map<juce::String, float> snapshot (Kind k) const
    {
        std::map<juce::String, float> values;
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (k == Kind::Sequence && ! isSequenceParam (rp->paramID)) continue;
                values[rp->paramID] = rp->convertFrom0to1 (rp->getValue());
            }
        return values;
    }

    // applique les valeurs ; ce qui manque revient à la valeur par défaut. À appeler depuis le fil de l'interface.
    void apply (Kind k, const std::map<juce::String, float>& values)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (k == Kind::Sequence && ! isSequenceParam (rp->paramID)) continue;
                const auto it = values.find (rp->paramID);
                const float real = it != values.end() ? it->second : rp->convertFrom0to1 (rp->getDefaultValue());
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->convertTo0to1 (real));
                rp->endChangeGesture();
            }
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::map<juce::String, float> slot[2];
    bool slotValid[2] = { false, false };
    int active = 0;
};
} // namespace bbdpresets
