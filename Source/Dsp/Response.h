#pragma once

// Modèle analytique de la réponse en fréquence du résonateur, pour la courbe de l'interface.
// Mêmes formules que Resonator::update() (filtres, BBD, compensation de phase) : le test
// ResonatorTest.cpp compare ce modèle à la réponse mesurée du vrai résonateur.
// Hypothèses : Drive à zéro, Cross à zéro, pas de modulation (LFO, Drift), signal faible (linéaire).

#include <complex>
#include "Resonator.h"

namespace bbdres
{
struct ResponseParams
{
    float semitones = 48.0f;
    float feedback = 0.7f;
    float inputLpHz = 8000.0f;
    float loopLpHz = 12000.0f;
    float bbd = 0.0f;
    float fs = 48000.0f;
};

class ResponseModel
{
public:
    void set (const ResponseParams& p)
    {
        fs = p.fs;
        f0 = 440.0f * std::exp2 ((p.semitones - 69.0f) / 12.0f);
        const float fb = std::clamp (p.feedback, 0.0f, 1.0f);
        g = fb * Resonator::kMaxLoopGain;
        inGain = 1.0f - 0.8f * fb;
        loop.setCutoff (p.loopLpHz, fs);
        inLp.setLowpass (p.inputLpHz, fs);

        const float bbdAmt = std::clamp (p.bbd, 0.0f, 1.0f);
        const float R = 770.0f * std::exp2 (-5.5f * bbdAmt);
        bw.setLowpass (std::min (0.2f * R * f0, 0.45f * fs), fs);
        bwMix = std::clamp (bbdAmt * 20.0f, 0.0f, 1.0f);

        // Même compensation de phase que le résonateur : la boucle est accordée sur f0
        const float w = 2.0f * kPi * f0 / fs;
        const std::complex<float> h = (1.0f - bwMix) + bwMix * bw.response (f0, fs);
        const float pd = loop.phaseDelay (f0, fs) - std::arg (h) / w;
        delaySamples = std::clamp (fs / f0 - pd, 2.0f, 1.0e6f);
    }

    float fundamentalHz() const { return f0; }

    // Réponse en amplitude (dB) de la sortie humide à la fréquence f
    float db (float f) const
    {
        const float w = 2.0f * kPi * f / fs;
        const std::complex<float> lp = (1.0f - loop.a) / (1.0f - loop.a * std::polar (1.0f, -w));
        const std::complex<float> b = lp * ((1.0f - bwMix) + bwMix * bw.response (f, fs));
        const std::complex<float> z = std::polar (1.0f, -w * delaySamples);
        const std::complex<float> h = inGain * inLp.response (f, fs) * b * z / (1.0f - g * b * z);
        return 20.0f * std::log10 (std::max (std::abs (h), 1.0e-6f));
    }

private:
    float fs = 48000.0f, f0 = 130.0f, g = 0.0f, inGain = 1.0f, bwMix = 0.0f, delaySamples = 100.0f;
    OnePole loop;
    Biquad inLp, bw;
};
} // namespace bbdres
