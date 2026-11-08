#pragma once

// Sources de modulation, appelées à la cadence de contrôle (C++ standard uniquement).

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Resonator.h"

namespace bbdres
{
// LFO : 0 Sine, 1 Triangle, 2 Square, 3 Ramp, 4 Random (échantillonné-bloqué), 5 Smooth Random
class Lfo
{
public:
    void prepare (float controlRate, std::uint32_t seed)
    {
        rate = controlRate;
        rng = seed * 2654435761u + 977u;
        phase = 0.0f;
        prevR = rand01();
        nextR = rand01();
    }
    void setRateHz (float hz) { inc = std::clamp (hz, 0.001f, 100.0f) / rate; }

    // Avance d'un pas de contrôle (fréquence libre). Renvoie une valeur entre -1 et +1
    float next (int shape)
    {
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            prevR = nextR;
            nextR = rand01();
        }
        return value (shape);
    }

    // Variante calée sur le temps : la phase (0..1) est imposée par l'hôte
    float nextAt (int shape, float newPhase)
    {
        if (newPhase < phase - 0.5f)   // la phase a bouclé
        {
            prevR = nextR;
            nextR = rand01();
        }
        phase = newPhase;
        return value (shape);
    }

private:
    float value (int shape) const
    {
        switch (shape)
        {
            case 0:  return std::sin (2.0f * 3.14159265f * phase);
            case 1:  return 4.0f * std::fabs (phase - 0.5f) - 1.0f;
            case 2:  return phase < 0.5f ? 1.0f : -1.0f;
            case 3:  return 2.0f * phase - 1.0f;
            case 4:  return prevR;
            default:
            {
                const float t = phase * phase * (3.0f - 2.0f * phase);  // lissage
                return prevR + (nextR - prevR) * t;
            }
        }
    }

    float rand01()  // -1..+1
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (std::int32_t) rng * (1.0f / 2147483648.0f);
    }
    float rate = 3000.0f, inc = 0.0f, phase = 0.0f, prevR = 0.0f, nextR = 0.0f;
    std::uint32_t rng = 1u;
};

// Synchronisation au tempo : 0 = libre, sinon durée d'un cycle en temps (noires)
inline constexpr const char* kSyncNames[] = { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4",
                                              "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16" };
inline constexpr float kSyncBeats[] = { 0.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f,
                                        1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f };
inline constexpr int kNumSync = (int) (sizeof (kSyncBeats) / sizeof (kSyncBeats[0]));

// Longueur du looper : curseur 0..1 -> 30 ms à 4 s (échelle exponentielle)
inline float holdSecondsFromSlider (float x)
{
    return kHoldMinSeconds * std::pow (kHoldMaxSeconds / kHoldMinSeconds, std::clamp (x, 0.0f, 1.0f));
}

// Longueur de boucle rythmique : durées en temps (noires), de la triple croche à 4 mesures
inline constexpr float kHoldBeats[] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 16.0f };

// Durée rythmique la plus proche (en échelle logarithmique), comprise dans la mémoire du looper
inline float quantizeHoldSeconds (float seconds, float bpm)
{
    const float spb = 60.0f / std::max (bpm, 20.0f);
    float best = std::clamp (seconds, kHoldMinSeconds, kHoldMaxSeconds), bestDist = 1.0e9f;
    for (const float beats : kHoldBeats)
    {
        const float s = beats * spb;
        if (s < kHoldMinSeconds || s > kHoldMaxSeconds) continue;
        const float dist = std::fabs (std::log (s / seconds));
        if (dist < bestDist) { bestDist = dist; best = s; }
    }
    return best;
}

// Phase (0..1) d'un LFO calé sur la position de l'hôte, exprimée en noires
inline float syncedPhase (double ppq, float beatsPerCycle)
{
    const double c = ppq / (double) beatsPerCycle;
    return (float) (c - std::floor (c));
}

// Suiveur d'enveloppe : attaque 10 ms, relâchement 200 ms
struct EnvFollower
{
    float y = 0.0f, aA = 1.0f, aR = 1.0f;
    void prepare (float controlRate)
    {
        aA = 1.0f - std::exp (-1.0f / (0.010f * controlRate));
        aR = 1.0f - std::exp (-1.0f / (0.200f * controlRate));
    }
    float next (float level) { y += (level > y ? aA : aR) * (level - y); return y; }
};

// Niveau linéaire -> 0..1 (-40 dBFS = 0, 0 dBFS = 1)
inline float levelToUnit (float level)
{
    const float db = 20.0f * std::log10 (std::max (level, 1.0e-5f));
    return std::clamp ((db + 40.0f) / 40.0f, 0.0f, 1.0f);
}
} // namespace bbdres
