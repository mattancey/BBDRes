#pragma once
 
// Gammes et quantification de la hauteur (C++ standard uniquement).
 
#include <cmath>
 
namespace bbdres
{
// Masque de bits : le bit k est à 1 si le degré k (en demi-tons au-dessus de la tonique) est permis
template <typename... T>
constexpr unsigned scaleMask (T... degrees)
{
    return ((1u << degrees) | ...);
}
 
struct ScaleDef
{
    const char* name;
    unsigned mask;
};
 
inline constexpr ScaleDef kScales[] = {
    { "Chromatic",  scaleMask (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11) },
    { "Major",      scaleMask (0, 2, 4, 5, 7, 9, 11) },
    { "Minor",      scaleMask (0, 2, 3, 5, 7, 8, 10) },
    { "Dorian",     scaleMask (0, 2, 3, 5, 7, 9, 10) },
    { "Phrygian",   scaleMask (0, 1, 3, 5, 7, 8, 10) },
    { "Lydian",     scaleMask (0, 2, 4, 6, 7, 9, 11) },
    { "Mixolydian", scaleMask (0, 2, 4, 5, 7, 9, 10) },
    { "Harm Minor", scaleMask (0, 2, 3, 5, 7, 8, 11) },
    { "Pent Major", scaleMask (0, 2, 4, 7, 9) },
    { "Pent Minor", scaleMask (0, 3, 5, 7, 10) },
    { "Whole Tone", scaleMask (0, 2, 4, 6, 8, 10) },
    { "Root + 5th", scaleMask (0, 7) },
};
inline constexpr int kNumScales = (int) (sizeof (kScales) / sizeof (kScales[0]));
 
// Renvoie la note de la gamme la plus proche (à égalité, la plus grave)
inline float quantizeToScale (float note, int root, unsigned mask)
{
    if (mask == 0u)
        return note;
 
    const int center = (int) std::floor (note + 0.5f);
    float best = note, bestDist = 1.0e9f;
    for (int k = -6; k <= 6; ++k)
    {
        const int c = center + k;
        const int degree = (((c - root) % 12) + 12) % 12;
        if ((mask >> degree) & 1u)
        {
            const float dist = std::fabs (note - (float) c);
            if (dist < bestDist) { bestDist = dist; best = (float) c; }
        }
    }
    return best;
}
} // namespace bbdres
