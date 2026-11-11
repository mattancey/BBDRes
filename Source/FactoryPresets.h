#pragma once

// Presets d'usine : des POINTS DE DÉPART (réglés sur le papier et par mesures, pas à l'oreille).
// Un preset ne liste que ce qui diffère des valeurs par défaut ; le reste y revient au chargement.
// C++ standard uniquement : les valeurs sont vérifiées par ResonatorTest.cpp.

#include <initializer_list>
#include <vector>
#include "Params.h"

namespace bbdpresets
{
struct PV
{
    const char* id;
    float value;   // valeur réelle du paramètre (index pour un choix, 0/1 pour un interrupteur)
};

struct FactoryPreset
{
    const char* name;
    std::vector<PV> values;
};

// Presets de séquence : longueur, durée d'un pas (index dans kSeqSteps), glide, Q SCALE, puis les hauteurs
inline FactoryPreset sequence (const char* name, int length, int stepIndex, float glide, bool quant, std::initializer_list<float> steps)
{
    FactoryPreset p { name, {} };
    p.values.push_back ({ ParamIDs::seqLength, (float) length });
    p.values.push_back ({ ParamIDs::seqStep, (float) stepIndex });
    p.values.push_back ({ ParamIDs::seqGlide, glide });
    p.values.push_back ({ ParamIDs::seqQuant, quant ? 1.0f : 0.0f });
    int i = 0;
    for (const float v : steps)
    {
        if (i < 16) p.values.push_back ({ ParamIDs::kStepIDs[i], v });
        ++i;
    }
    return p;
}

inline const std::vector<FactoryPreset>& sequenceFactory()
{
    static const std::vector<FactoryPreset> list = []
    {
        std::vector<FactoryPreset> v;
        v.push_back (sequence ("Init", 8, 5, 0.0f, true, {}));
        v.push_back (sequence ("Octaves", 8, 2, 0.0f, true, { 0, 12, 0, 12, 0, 12, 0, 12 }));
        v.push_back (sequence ("Fifths Climb", 8, 2, 0.1f, true, { 0, 7, 12, 7, 0, 7, 12, 19 }));
        v.push_back (sequence ("Minor Walk", 8, 2, 0.2f, true, { 0, 3, 7, 10, 12, 10, 7, 3 }));
        v.push_back (sequence ("Half-Bar Steps", 8, 9, 0.3f, true, { 0, 7, 5, 3, 0, -5, -2, 3 }));
        v.push_back (sequence ("Slow Drift", 4, 10, 0.8f, true, { 0, 5, 3, 7 }));
        v.push_back (sequence ("Slow Tide", 4, 11, 1.0f, true, { 0, -5, -3, -7 }));
        return v;
    }();
    return list;
}

inline const std::vector<FactoryPreset>& generalFactory()
{
    static const std::vector<FactoryPreset> list = []
    {
        std::vector<FactoryPreset> v;
        v.push_back ({ "Init", {} });

        v.push_back ({ "Pad Cathedral", {
            { ParamIDs::feedback, 0.82f }, { ParamIDs::loopLP, 9000.0f }, { ParamIDs::loopDyn, 0.25f }, { ParamIDs::bbd, 0.4f },
            { ParamIDs::lowCut, 90.0f }, { ParamIDs::pultec, 0.3f }, { ParamIDs::drive, 0.15f }, { ParamIDs::drift, 0.45f },
            { ParamIDs::spread, 0.4f }, { ParamIDs::cross, 0.3f }, { ParamIDs::mix, 0.5f } } });

        v.push_back ({ "Dark Drone", {
            { ParamIDs::tune, 40.0f }, { ParamIDs::feedback, 0.94f }, { ParamIDs::loopLP, 3500.0f }, { ParamIDs::bbd, 0.7f },
            { ParamIDs::noise, 0.12f }, { ParamIDs::lowCut, 140.0f }, { ParamIDs::pultec, 0.55f }, { ParamIDs::drift, 0.6f },
            { ParamIDs::spread, 0.6f }, { ParamIDs::cross, 0.5f }, { ParamIDs::lfoShape, 5.0f }, { ParamIDs::lfoTarget, 2.0f },
            { ParamIDs::lfoDepth, 0.35f }, { ParamIDs::lfoRate, 0.12f }, { ParamIDs::holdLen, 0.7f }, { ParamIDs::holdHP, 120.0f },
            { ParamIDs::holdLP, 5000.0f }, { ParamIDs::mix, 0.6f } } });

        v.push_back ({ "Frozen Choir", {
            { ParamIDs::feedback, 0.75f }, { ParamIDs::loopLP, 7000.0f }, { ParamIDs::bbd, 0.3f }, { ParamIDs::noise, 0.05f },
            { ParamIDs::drift, 0.5f }, { ParamIDs::spread, 0.5f }, { ParamIDs::holdLen, 0.62f }, { ParamIDs::holdMix, 1.0f },
            { ParamIDs::holdHP, 150.0f }, { ParamIDs::holdLP, 6000.0f }, { ParamIDs::mix, 0.45f } } });

        FactoryPreset arp { "Glass Arp", {
            { ParamIDs::tune, 60.0f }, { ParamIDs::feedback, 0.88f }, { ParamIDs::loopLP, 12000.0f }, { ParamIDs::bbd, 0.2f },
            { ParamIDs::root, 0.0f }, { ParamIDs::scale, 2.0f }, { ParamIDs::mix, 0.55f }, { ParamIDs::seqOn, 1.0f } } };
        for (const auto& pv : sequence ("", 8, 2, 0.2f, true, { 0, 3, 7, 10, 12, 10, 7, 3 }).values)
            arp.values.push_back (pv);
        v.push_back (arp);
        return v;
    }();
    return list;
}
} // namespace bbdpresets
