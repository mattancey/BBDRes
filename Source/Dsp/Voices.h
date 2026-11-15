#pragma once

// Résonateurs multiples : logique pure (C++ standard), testée hors du plug-in.
// Quatre voix en parallèle. Chacune a un intervalle par rapport à Tune, un niveau, une balance et un Solo.

#include <algorithm>
#include "Pitch.h"

namespace bbdres
{
constexpr int kNumVoices = 4;

struct PanGains { float l, r; };

// Balance (et non panoramique) : la voix est stéréo, on atténue seulement le côté opposé. 0 = centre, aucun affaiblissement.
inline PanGains panGains (float pan)
{
    pan = std::clamp (pan, -1.0f, 1.0f);
    return { pan > 0.0f ? 1.0f - pan : 1.0f, pan < 0.0f ? 1.0f + pan : 1.0f };
}

// Solo : dès qu'une voix est en solo, seules les voix en solo s'entendent (même si elles étaient éteintes)
inline bool voiceAudible (bool on, bool solo, bool anySolo)
{
    return anySolo ? solo : on;
}

// Hauteur d'une voix (demi-tons) : base (séquenceur + LFO) + intervalle ; avec Quantize, la note est calée sur la
// gamme, donc l'accord reste dans la gamme ; puis l'accord fin.
inline float voicePitch (float baseRaw, float interval, float fineSemis, bool quantize, int root, unsigned mask)
{
    float p = baseRaw + interval;
    if (quantize)
        p = quantizeToScale (p, root, mask);
    return p + fineSemis;
}
} // namespace bbdres
