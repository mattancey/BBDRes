#pragma once
 
// Sources de modulation, appelées à la cadence de contrôle (C++ standard uniquement).
 
#include <algorithm>
#include <cmath>
#include <cstdint>
 
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
 
    // Avance d'un pas de contrôle. Renvoie une valeur entre -1 et +1
    float next (int shape)
    {
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            prevR = nextR;
            nextR = rand01();
        }
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
 
private:
    float rand01()  // -1..+1
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (std::int32_t) rng * (1.0f / 2147483648.0f);
    }
    float rate = 3000.0f, inc = 0.0f, phase = 0.0f, prevR = 0.0f, nextR = 0.0f;
    std::uint32_t rng = 1u;
};
 
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
