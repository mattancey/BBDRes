#pragma once

// Séquenceur de pitch : logique pure (C++ standard), testée hors du plug-in.
// Chaque pas porte un décalage en demi-tons par rapport à Tune. Un pas désactivé garde la hauteur
// du pas précédent (liaison). Le glissando est une fraction de la durée du pas.

#include <algorithm>
#include <cmath>

namespace bbdres
{
constexpr int kMaxSeqSteps = 16;

// Durée d'un pas : en temps (noires), ou en fraction de mesure (selon la signature rythmique de l'hôte)
struct SeqStepDef
{
    const char* name;
    float beats;
    float bars;
};

inline constexpr SeqStepDef kSeqSteps[] = {
    { "1/16", 0.25f, 0.0f },
    { "1/8T", 1.0f / 3.0f, 0.0f },
    { "1/8", 0.5f, 0.0f },
    { "1/8.", 0.75f, 0.0f },
    { "1/4T", 2.0f / 3.0f, 0.0f },
    { "1/4", 1.0f, 0.0f },
    { "1/4.", 1.5f, 0.0f },
    { "1/2", 2.0f, 0.0f },
    { "1/2.", 3.0f, 0.0f },
    { "1/2 bar", 0.0f, 0.5f },
    { "1 bar", 0.0f, 1.0f },
    { "2 bars", 0.0f, 2.0f },
    { "4 bars", 0.0f, 4.0f },
};
inline constexpr int kNumSeqSteps = (int) (sizeof (kSeqSteps) / sizeof (kSeqSteps[0]));

inline double seqStepBeats (int index, double barBeats)
{
    const auto& d = kSeqSteps[std::clamp (index, 0, kNumSeqSteps - 1)];
    return (double) d.beats + (double) d.bars * barBeats;
}

struct SeqState
{
    float prev = 0.0f;   // hauteur (demi-tons) du pas précédent
    float cur = 0.0f;    // hauteur du pas en cours
    float t = 1.0f;      // avancement du glissando, de 0 (précédent) à 1 (en cours)
    int index = 0;       // pas en cours
};

// posSteps : position en pas (négative ou très grande possible). glide : 0 = saut franc, 1 = glisse sur tout le pas.
inline SeqState sequencerState (double posSteps, int length, const float* values, const bool* on, float glide)
{
    length = std::clamp (length, 1, kMaxSeqSteps);
    const double fl = std::floor (posSteps);
    const double frac = posSteps - fl;
    long long m = (long long) fl % (long long) length;
    if (m < 0) m += length;
    const int idx = (int) m;

    // hauteur effective d'un pas : un pas désactivé reprend celle du précédent actif
    auto effective = [&] (int i)
    {
        for (int k = 0; k < length; ++k)
        {
            const int j = ((i - k) % length + length) % length;
            if (on[j]) return values[j];
        }
        return 0.0f;
    };

    SeqState s;
    s.index = idx;
    s.cur = effective (idx);
    s.prev = effective ((idx + length - 1) % length);
    s.t = glide <= 0.0001f ? 1.0f : (float) std::min (1.0, frac / (double) glide);
    return s;
}
} // namespace bbdres
