#pragma once

// Analyseur à Q constant (CQT) : 24 bins par octave (quarts de ton), 8 octaves, de do0 (32,7 Hz) à environ 8,1 kHz.
// C++ standard uniquement, testé hors du plug-in. Calcul direct dans le temps, noyaux de Hann alignés sur
// l'échantillon le PLUS RÉCENT : les aigus réagissent tout de suite, les graves intègrent plus longtemps
// (fenêtre limitée à 0,5 s). Un sinus d'amplitude A sur un bin donne A en amplitude, soit 20 log10 (A) en dB.

#include <algorithm>
#include <cmath>
#include <vector>

namespace bbdres
{
struct Rgb { unsigned char r, g, b; };

class Cqt
{
public:
    static constexpr int kBinsPerOctave = 24;
    static constexpr int kOctaves = 8;
    static constexpr int kNumBins = kBinsPerOctave * kOctaves;
    static constexpr float kMinHz = 32.7032f;          // do0 dans la notation de Live (do3 = 60)
    static constexpr float kMaxWindowSeconds = 0.5f;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const double pi = 3.14159265358979323846;
        const double q = 1.0 / (std::pow (2.0, 1.0 / kBinsPerOctave) - 1.0);
        offset.assign ((size_t) kNumBins, 0);
        length.assign ((size_t) kNumBins, 0);
        norm.assign ((size_t) kNumBins, 1.0f);
        cosK.clear();
        sinK.clear();
        for (int k = 0; k < kNumBins; ++k)
        {
            const double f = (double) binHz (k);
            const int m = std::clamp ((int) std::ceil (q * (double) fs / f), 16, (int) (kMaxWindowSeconds * fs));
            length[(size_t) k] = m;
            offset[(size_t) k] = (int) cosK.size();
            const double w0 = 2.0 * pi * f / (double) fs;
            double sumW = 0.0;
            for (int i = 0; i < m; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) i / (double) (m - 1));
                cosK.push_back ((float) (w * std::cos (w0 * (double) i)));
                sinK.push_back ((float) (w * std::sin (w0 * (double) i)));
                sumW += w;
            }
            norm[(size_t) k] = (float) (2.0 / sumW);
        }
        maxLen = *std::max_element (length.begin(), length.end());
    }

    static float binHz (int k) { return kMinHz * std::exp2 ((float) k / (float) kBinsPerOctave); }
    int maxLength() const { return maxLen; }   // nombre d'échantillons récents nécessaires

    // x : les n derniers échantillons (n >= maxLength()), le plus récent en dernier. outDb : kNumBins valeurs en dB.
    void analyze (const float* x, int n, float* outDb) const
    {
        for (int k = 0; k < kNumBins; ++k)
        {
            const int m = length[(size_t) k];
            const float* c = cosK.data() + offset[(size_t) k];
            const float* s = sinK.data() + offset[(size_t) k];
            const float* xs = x + (n - m);
            float re = 0.0f, im = 0.0f;
            for (int i = 0; i < m; ++i)
            {
                re += xs[i] * c[i];
                im += xs[i] * s[i];
            }
            outDb[k] = 20.0f * std::log10 (std::max (norm[(size_t) k] * std::sqrt (re * re + im * im), 1.0e-6f));
        }
    }

private:
    float fs = 48000.0f;
    int maxLen = 0;
    std::vector<int> offset, length;
    std::vector<float> norm, cosK, sinK;
};

// Couleur d'un bin : la teinte suit la classe de hauteur (do = rouge, la boucle chaque octave, les quarts de ton
// donnent les teintes intermédiaires) ; la luminosité suit le niveau, avec un contraste marqué (noir profond
// sous -85 dB), et les pics les plus forts tirent vers le blanc.
inline Rgb cqtColour (int bin, float db)
{
    const float hue = std::fmod ((float) bin / (float) Cqt::kBinsPerOctave, 1.0f);
    float v = std::clamp ((db + 85.0f) / 80.0f, 0.0f, 1.0f);
    v = v * v;
    const float t = std::clamp ((v - 0.8f) / 0.2f, 0.0f, 1.0f);
    const float s = 0.92f - 0.5f * t * t * (3.0f - 2.0f * t);
    const float h6 = hue * 6.0f;
    const int i = (int) h6;
    const float f = h6 - (float) i;
    const float p = v * (1.0f - s), q = v * (1.0f - s * f), u = v * (1.0f - s * (1.0f - f));
    float r = v, g = u, b = p;
    switch (i % 6)
    {
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = u; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = u; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: break;
    }
    return { (unsigned char) (r * 255.0f), (unsigned char) (g * 255.0f), (unsigned char) (b * 255.0f) };
}
} // namespace bbdres
