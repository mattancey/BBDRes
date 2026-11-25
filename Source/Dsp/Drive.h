#pragma once

// Saturation d'entrée, calculée UNE FOIS par canal (et non plus dans chaque voix), avec suréchantillonnage x2 permanent.
//
// Pourquoi : la saturation (surtout Fold) produit des harmoniques bien au-delà de Nyquist ; sans suréchantillonnage elles
// se replient dans la bande audible, et le filtre d'entrée qui suit ne peut plus les retirer. À 2 x la fréquence, ces
// harmoniques sont retirées par le filtre de descente au lieu de se replier.
//
// Le suréchantillonneur est un demi-bande polyphasé à déphaseurs (7 coefficients, conçu d'après un filtre elliptique) :
// rejet d'environ 99 dB, bande passante plate jusqu'à 0,44 fois la fréquence d'échantillonnage, latence d'environ 3
// échantillons (non constante en fréquence : phase minimale, rien à compenser). C++ standard uniquement.
//
// Aux fréquences d'échantillonnage de 80 kHz et plus, il n'est pas utilisé : l'aliasing y est déjà hors de la bande audible.

#include <algorithm>
#include <cmath>

namespace bbdres
{
// Les trois courbes de saturation : 0 douce, 1 asymétrique, 2 repli d'onde
inline float driveShape (int curve, float x)
{
    switch (curve)
    {
        case 1:  return std::tanh (x + 0.4f) - 0.37994896f;   // asymétrique (tanh(0.4))
        case 2:  return std::sin (x);                          // repli d'onde
        default: return std::tanh (x);                         // douce
    }
}

// Coefficients des deux branches du demi-bande (déphaseurs du premier ordre en z^2)
inline constexpr double kHalfBandA0[4] = { 0.041072550654970778, 0.30630294027097982, 0.63350454113178378, 0.92545694609615115 };
inline constexpr double kHalfBandA1[3] = { 0.15253331940310816, 0.47301898579208568, 0.78216438802015265 };

struct AllpassChain
{
    const double* a = nullptr;
    int n = 0;
    double x1[4] = {}, y1[4] = {};
    void reset() { for (int i = 0; i < 4; ++i) x1[i] = y1[i] = 0.0; }
    double process (double x)
    {
        for (int i = 0; i < n; ++i)
        {
            const double y = a[i] * (x - y1[i]) + x1[i];
            x1[i] = x;
            y1[i] = y;
            x = y;
        }
        return x;
    }
};

// 1 -> 2 : une entrée, deux sorties (échantillon pair puis impair à la fréquence double)
class HalfBandUp
{
public:
    HalfBandUp() { even.a = kHalfBandA0; even.n = 4; odd.a = kHalfBandA1; odd.n = 3; }
    void reset() { even.reset(); odd.reset(); }
    void process (double x, double& outEven, double& outOdd) { outEven = even.process (x); outOdd = odd.process (x); }

private:
    AllpassChain even, odd;
};

// 2 -> 1 : deux entrées (pair puis impair), une sortie
class HalfBandDown
{
public:
    HalfBandDown() { even.a = kHalfBandA0; even.n = 4; odd.a = kHalfBandA1; odd.n = 3; }
    void reset() { even.reset(); odd.reset(); prevOdd = 0.0; }
    double process (double inEven, double inOdd)
    {
        const double r = 0.5 * (even.process (inEven) + odd.process (prevOdd));
        prevOdd = inOdd;
        return r;
    }

private:
    AllpassChain even, odd;
    double prevOdd = 0.0;
};

class DriveStage
{
public:
    static constexpr float kMaxOversampledRate = 80000.0f;

    void prepare (float sampleRate)
    {
        oversampled = sampleRate < kMaxOversampledRate;
        up.reset();
        down.reset();
    }
    bool isOversampled() const { return oversampled; }

    // Mêmes formules que dans Resonator::update : gain 1..16, compensation, dosage progressif sous 10 % de drive
    void update (float drive, int curveIndex)
    {
        const float dr = std::clamp (drive, 0.0f, 1.0f);
        gain = 1.0f + 15.0f * dr;
        comp = 1.0f / std::sqrt (gain);
        mix = std::clamp (dr * 10.0f, 0.0f, 1.0f);
        curve = std::clamp (curveIndex, 0, 2);
    }

    float process (float x)
    {
        if (! oversampled) return shaped (x);
        double e, o;
        up.process ((double) x, e, o);
        const float fe = shaped ((float) e), fo = shaped ((float) o);   // la formule entière passe par le suréchantillonneur : latence fixe, quelle que soit la saturation
        return (float) down.process ((double) fe, (double) fo);   // la descente garde elle-même l'échantillon impair précédent
    }

private:
    float shaped (float x) const
    {
        return mix > 0.0f ? x + (driveShape (curve, x * gain) * comp - x) * mix : x;
    }

    bool oversampled = true;
    float gain = 1.0f, comp = 1.0f, mix = 0.0f;
    int curve = 0;
    HalfBandUp up;
    HalfBandDown down;
};
} // namespace bbdres
