#pragma once

// Coeur DSP de BBDRes : C++ standard uniquement (aucune dépendance JUCE),
// pour pouvoir être testé séparément du plug-in.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace bbdres
{
constexpr float kPi = 3.14159265358979323846f;

// Lissage à la cadence de contrôle (un appel par sous-bloc)
struct Smoother
{
    float y = 0.0f, coef = 1.0f;
    void prepare (float controlRate, float tauSeconds)
    {
        coef = 1.0f - std::exp (-1.0f / (tauSeconds * controlRate));
    }
    void snap (float v) { y = v; }
    float next (float target) { y += coef * (target - y); return y; }
};

// Passe-bas 12 dB/oct (Butterworth), forme transposée DF2
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (float fc, float fs)
    {
        fc = std::clamp (fc, 10.0f, 0.45f * fs);
        const float w0 = 2.0f * kPi * fc / fs;
        const float cw = std::cos (w0), sw = std::sin (w0);
        const float alpha = sw / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - cw) * 0.5f / a0;
        b1 = (1.0f - cw) / a0;
        b2 = b0;
        a1 = -2.0f * cw / a0;
        a2 = (1.0f - alpha) / a0;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }

    // Réponse complexe à la fréquence f
    std::complex<float> response (float f, float fs) const
    {
        const float w = 2.0f * kPi * f / fs;
        const std::complex<float> zi = std::polar (1.0f, -w);
        const std::complex<float> z2c = zi * zi;
        return (b0 + b1 * zi + b2 * z2c) / (1.0f + a1 * zi + a2 * z2c);
    }

    // Retard de phase (en échantillons) à la fréquence f
    float phaseDelay (float f, float fs) const
    {
        const float w = 2.0f * kPi * f / fs;
        if (w < 1.0e-6f) return 0.0f;
        return -std::arg (response (f, fs)) / w;
    }
};

// Passe-bas 6 dB/oct
struct OnePole
{
    float a = 0.0f, y1 = 0.0f;

    void setCutoff (float fc, float fs)
    {
        fc = std::clamp (fc, 10.0f, 0.45f * fs);
        a = std::exp (-2.0f * kPi * fc / fs);
    }
    float process (float x) { y1 = (1.0f - a) * x + a * y1; return y1; }
    void reset() { y1 = 0.0f; }

    float phaseDelay (float f, float fs) const
    {
        const float w = 2.0f * kPi * f / fs;
        if (w < 1.0e-6f) return a / (1.0f - a);
        return std::atan2 (a * std::sin (w), 1.0f - a * std::cos (w)) / w;
    }
};

// Bloqueur de continu (5 Hz), placé à l'entrée uniquement
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.999f;
    void prepare (float fs) { r = 1.0f - 2.0f * kPi * 5.0f / fs; }
    float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// Ligne à retard avec lecture fractionnaire (interpolation cubique de Hermite)
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // Lecture AVANT push : un retard de d échantillons (d >= 2)
    float read (float d) const
    {
        const int di = (int) d;
        const float x = 1.0f - (d - (float) di);
        const float ym1 = at (di + 2), y0 = at (di + 1), y1 = at (di), y2 = at (di - 1);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * x + c2) * x + c1) * x + y0;
    }
    int size() const { return mask + 1; }

private:
    float at (int k) const { return buf[(size_t) ((w - k) & mask)]; }
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// Réglages d'un résonateur (valeurs déjà lissées)
struct ResonatorParams
{
    float semitones = 48.0f;   // note MIDI (fractionnaire possible)
    float feedback = 0.7f;     // 0..1
    float inputLpHz = 8000.0f; // filtre d'entrée 12 dB
    float loopLpHz = 6000.0f;  // filtre de boucle 6 dB
    float drive = 0.0f;        // 0..1
    int curve = 0;             // 0 = Soft, 1 = Asym, 2 = Fold
    float bbd = 0.0f;          // 0..1 : caractère BBD (bande passante liée à la note + grain)
    float noise = 0.0f;        // 0..1 : souffle de la ligne BBD (filtré comme le reste)
    float cross = 0.0f;        // 0..1 : couplage avec l'autre canal
    float freeze = 0.0f;       // > 0.5 : fige le son (hold)
    float holdLen = 0.5f;      // 0..1 : mémoire du hold, de 50 ms à 1 s (échelle exponentielle)
    float noiseGain = 1.0f;    // réglage interne : multiplicateur du souffle
    float jitterGain = 1.0f;   // réglage interne : multiplicateur du jitter d'horloge
};

// Résonateur : filtre en peigne à feedback positif, accordé par le temps de retard.
// La sortie humide est prise à la SORTIE DE LA LIGNE BBD (après le souffle et les filtres de boucle) :
// le BBD colore donc tout le signal humide, et le signal sec revient par Mix.
//
// Freeze (hold) : la sortie brute de la ligne est enregistrée en continu. Au gel, les N derniers
// échantillons (N = 4 x un nombre entier de périodes) sont rejoués par QUATRE lecteurs décalés de N/4,
// chacun fenêtré en Hann. La somme des fenêtres est constante : il n'y a ni point de bouclage
// audible ni pulsation, et le son reste cohérent en hauteur. La vitesse de lecture suit la note
// (Tune, LFO, Drift, Spread) : le son figé se joue comme un oscillateur. Il repasse par les filtres
// de boucle et le BBD en direct.
class Resonator
{
public:
    static constexpr float kMaxLoopGain = 1.02f;       // > 1 : auto-oscillation possible
    static constexpr float kMinFreqHz = 20.0f;
    static constexpr float kHoldMinSeconds = 0.05f;    // mémoire du hold : holdLen = 0
    static constexpr float kHoldMaxSeconds = 1.0f;     // mémoire du hold : holdLen = 1

    void prepare (float sampleRate, std::uint32_t seed = 1u)
    {
        fs = sampleRate;
        rng = seed * 2654435761u + 12345u;
        delay.prepare ((int) (fs / kMinFreqHz) + 8);
        maxDelay = (float) delay.size() - 6.0f;
        dc.prepare (fs);
        jitterLp.setCutoff (150.0f, fs);
        // Normalise le bruit de jitter filtré pour qu'il ait un écart-type d'environ 1
        const float a = jitterLp.a;
        jitterNorm = std::sqrt (3.0f * (1.0f + a) / (1.0f - a));

        // Enregistreur de la sortie brute de la ligne et mémoire de la boucle gelée
        const int cap = (int) (fs * (kHoldMaxSeconds + 0.2f));
        int n = 1;
        while (n < cap) n <<= 1;
        rec.assign ((size_t) n, 0.0f);
        recMask = n - 1;
        fzBuf.assign ((size_t) cap, 0.0f);
        fzCap = cap;
        fzSlew = 1.0f - std::exp (-1.0f / (0.03f * fs));

        update (ResonatorParams {});
        reset();
    }
    void reset()
    {
        delay.reset(); inLp.reset(); loop.reset(); dc.reset(); bw.reset(); jitterLp.reset(); lastLoop = 0.0f;
        loopF.reset(); bwF.reset();
        std::fill (rec.begin(), rec.end(), 0.0f);
        recW = 0; frozen = false; fzMix = 0.0f; fzLen = 0;
    }

    void update (const ResonatorParams& p)
    {
        const float fb = std::clamp (p.feedback, 0.0f, 1.0f);
        const float f0 = 440.0f * std::exp2 ((p.semitones - 69.0f) / 12.0f);
        f0Now = f0;
        periodSamples = fs / f0;
        loopGain = fb * kMaxLoopGain;
        inGain = 1.0f - 0.8f * fb;
        loop.setCutoff (p.loopLpHz, fs);
        loopF.a = loop.a;
        inLp.setLowpass (p.inputLpHz, fs);

        // Drive
        const float dr = std::clamp (p.drive, 0.0f, 1.0f);
        driveGain = 1.0f + 15.0f * dr;
        driveComp = 1.0f / std::sqrt (driveGain);
        driveMix = std::clamp (dr * 10.0f, 0.0f, 1.0f);
        curve = std::clamp (p.curve, 0, 2);

        // Caractère BBD : bande passante liée à l'horloge (donc à la note), bruit, jitter.
        // Une ligne de 2R étages bouclée sur une période 1/f0 échantillonne à R*f0 :
        // moins il y a d'étages (R petit), plus les graves sont sombres.
        // Le filtre tourne TOUJOURS (état chaud, fréquence continue) et son dosage glisse de 0 à 1
        // sur les premiers % du réglage : pas de clic au début de la course.
        const float bbdAmt = std::clamp (p.bbd, 0.0f, 1.0f);
        const float R = 770.0f * std::exp2 (-5.5f * bbdAmt);
        const float bwHz = std::min (0.2f * R * f0, 0.45f * fs);
        bwMix = std::clamp (bbdAmt * 20.0f, 0.0f, 1.0f);
        bw.setLowpass (bwHz, fs);
        bwF = bw; bwF.z1 = bwF.z2 = 0.0f;   // mêmes coefficients, état propre au chemin gelé
        const float w = 2.0f * kPi * f0 / fs;
        float pd = loop.phaseDelay (f0, fs);
        {
            const std::complex<float> h = (1.0f - bwMix) + bwMix * bw.response (f0, fs);
            pd += -std::arg (h) / w;
        }
        const float nz = std::clamp (p.noise, 0.0f, 1.0f);
        noiseAmp = 0.0025f * nz * nz * p.noiseGain;      // souffle injecté dans la ligne BBD
        jitterAmt = 0.00025f * bbdAmt * p.jitterGain;    // grain d'horloge

        // Couplage entre canaux : le chemin croisé a un échantillon de retard en plus, on le compense
        crossAmt = std::clamp (p.cross, 0.0f, 1.0f);
        if (crossAmt > 0.0f)
        {
            const std::complex<float> m = std::complex<float> (1.0f - crossAmt, 0.0f) + crossAmt * std::polar (1.0f, -w);
            pd += -std::arg (m) / w;
        }

        // Les filtres de la boucle retardent la phase : on les compense pour rester juste
        delaySamples = std::clamp (fs / f0 - pd, 2.0f, maxDelay);

        // Freeze : front montant = capture, front descendant = retour au signal direct
        holdSeconds = kHoldMinSeconds * std::pow (kHoldMaxSeconds / kHoldMinSeconds, std::clamp (p.holdLen, 0.0f, 1.0f));
        const bool wantFrozen = p.freeze > 0.5f;
        if (wantFrozen && ! frozen) captureLoop();
        else if (! wantFrozen)      frozen = false;
    }

    // crossV : sortie de boucle de l'autre canal (ignorée si cross = 0)
    float process (float x, float crossV = 0.0f)
    {
        // Drive (courbe choisie), puis filtre d'entrée, blocage du continu et gain
        float s = x;
        if (driveMix > 0.0f)
            s = x + (shape (x * driveGain) * driveComp - x) * driveMix;
        const float in = dc.process (inLp.process (s)) * inGain;

        // Jitter d'horloge : petites variations aléatoires du temps de retard
        float d = delaySamples;
        if (jitterAmt > 0.0f)
            d = std::clamp (d * (1.0f + jitterAmt * jitterLp.process (nextNoise()) * jitterNorm), 2.0f, maxDelay);

        // Sortie de la ligne BBD : le souffle y est ajouté, puis tout passe par les filtres de boucle
        float raw = delay.read (d);
        if (noiseAmp > 0.0f)
            raw += noiseAmp * nextNoise();
        rec[(size_t) recW] = raw;                 // enregistrement continu pour Freeze
        recW = (recW + 1) & recMask;

        float v = loop.process (raw);
        v += (bw.process (v) - v) * bwMix;
        lastLoop = v;
        const float vm = (1.0f - crossAmt) * v + crossAmt * crossV;
        const float y = in + std::tanh (loopGain * vm);  // saturation douce dans la boucle
        delay.push (y);

        // Sortie humide = sortie de la ligne (v), ou le son figé qui repasse par les mêmes filtres
        float out = v;
        if (frozen || fzMix > 0.0f)
        {
            float vf = loopF.process (readFrozen());
            vf += (bwF.process (vf) - vf) * bwMix;
            out += (vf - v) * fzMix;
            fzMix += ((frozen ? 1.0f : 0.0f) - fzMix) * fzSlew;
            if (! frozen && fzMix < 1.0e-4f) fzMix = 0.0f;
        }
        return out;
    }
    float loopOut() const { return lastLoop; }

private:
    // Interpolation cubique (Hermite) sur la boucle gelée, vue comme un cercle de n échantillons
    float frozenAt (float ph) const
    {
        const int n = fzLen;
        const int i0 = (int) ph;
        const float x = ph - (float) i0;
        const float ym1 = fzBuf[(size_t) ((i0 - 1 + n) % n)], y0 = fzBuf[(size_t) i0];
        const float y1 = fzBuf[(size_t) ((i0 + 1) % n)], y2 = fzBuf[(size_t) ((i0 + 2) % n)];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * x + c2) * x + c1) * x + y0;
    }
    float window (float ph) const
    {
        const float sn = std::sin (kPi * ph / (float) fzLen);
        return sn * sn;   // Hann : quatre fenêtres décalées de N/4 totalisent exactement 2
    }

    // Quatre lecteurs décalés de N/4, vitesse proportionnelle à la hauteur actuelle
    float readFrozen()
    {
        const float speed = std::clamp (f0Now / fzCapF0, 0.125f, 8.0f);
        const float len = (float) fzLen;
        float sum = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            float ph = fzPhase[k];
            sum += window (ph) * frozenAt (ph);
            ph += speed;
            while (ph >= len) ph -= len;
            fzPhase[k] = ph;
        }
        return sum * 0.5f * fzGain;
    }

    // Copie les N derniers échantillons (N = 4 x un nombre entier de périodes : les lecteurs restent en phase)
    void captureLoop()
    {
        const float period = std::max (periodSamples, 2.0f);
        int m = std::max (1, (int) std::lround (holdSeconds * fs / (4.0f * period)));
        int len = (int) std::lround (4.0f * (float) m * period);
        while (len > fzCap && m > 1) { --m; len = (int) std::lround (4.0f * (float) m * period); }
        len = std::clamp (len, 16, fzCap);
        for (int n = 0; n < len; ++n)
            fzBuf[(size_t) n] = rec[(size_t) ((recW - len + n) & recMask)];
        fzLen = len;
        fzCapF0 = f0Now;
        for (int k = 0; k < 4; ++k) fzPhase[k] = (float) k * (float) len * 0.25f;
        loopF.reset(); bwF.reset();

        // Gain de niveau : le son figé garde le niveau du son capturé, qu'il soit cohérent ou non
        // (la somme des quatre fenêtres vaut 2 en amplitude, mais seulement 1,22 en puissance
        // pour un contenu non corrélé). Mesure sur un tour de boucle, un échantillon sur 8.
        double pin = 0.0, pout = 0.0; int cnt = 0;
        for (int n = 0; n < len; n += 8)
        {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k)
            {
                const float ph = std::fmod ((float) n + (float) k * (float) len * 0.25f, (float) len);
                sum += (double) window (ph) * (double) fzBuf[(size_t) std::min ((int) ph, len - 1)];
            }
            pout += 0.25 * sum * sum;
            pin += (double) fzBuf[(size_t) n] * (double) fzBuf[(size_t) n];
            ++cnt;
        }
        fzGain = (pout > 1.0e-20 && pin > 1.0e-20) ? std::clamp ((float) std::sqrt (pin / pout), 0.5f, 4.0f) : 1.0f;
        frozen = true;
    }

    float shape (float x) const
    {
        switch (curve)
        {
            case 1:  return std::tanh (x + 0.4f) - 0.37994896f;  // asymétrique (tanh(0.4))
            case 2:  return std::sin (x);                          // repli d'onde
            default: return std::tanh (x);                         // douce
        }
    }
    float nextNoise()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (std::int32_t) rng * (1.0f / 2147483648.0f);
    }

    float fs = 48000.0f, maxDelay = 1000.0f, delaySamples = 100.0f, periodSamples = 100.0f, f0Now = 130.0f;
    float loopGain = 0.0f, inGain = 1.0f, crossAmt = 0.0f, lastLoop = 0.0f, bwMix = 0.0f;
    float driveGain = 1.0f, driveComp = 1.0f, driveMix = 0.0f;
    int curve = 0;
    float noiseAmp = 0.0f, jitterAmt = 0.0f, jitterNorm = 1.0f;
    std::uint32_t rng = 1u;
    DelayLine delay;
    Biquad inLp, bw, bwF;
    OnePole loop, loopF, jitterLp;
    DcBlocker dc;

    // Freeze
    std::vector<float> rec, fzBuf;
    int recMask = 0, recW = 0, fzLen = 0, fzCap = 0;
    bool frozen = false;
    float fzMix = 0.0f, fzSlew = 0.001f, fzGain = 1.0f, fzCapF0 = 130.0f, holdSeconds = 0.2f;
    float fzPhase[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};
} // namespace bbdres
