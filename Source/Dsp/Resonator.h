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

    // Retard de phase (en échantillons) à la fréquence f
    float phaseDelay (float f, float fs) const
    {
        const float w = 2.0f * kPi * f / fs;
        if (w < 1.0e-6f) return 0.0f;
        const std::complex<float> zi = std::polar (1.0f, -w);
        const std::complex<float> z2c = zi * zi;
        const std::complex<float> num = b0 + b1 * zi + b2 * z2c;
        const std::complex<float> den = 1.0f + a1 * zi + a2 * z2c;
        return -std::arg (num / den) / w;
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
    float freeze = 0.0f;       // > 0.5 : capture la sortie humide et la rejoue en boucle
    float noiseGain = 1.0f;    // réglage interne : multiplicateur du souffle
    float jitterGain = 1.0f;   // réglage interne : multiplicateur du jitter d'horloge
};

// Résonateur : filtre en peigne à feedback positif, accordé par le temps de retard.
// La sortie humide est prise à la SORTIE DE LA LIGNE BBD (après le souffle et les filtres de boucle) :
// le BBD colore donc tout le signal humide, et le signal sec revient par Mix.
class Resonator
{
public:
    static constexpr float kMaxLoopGain = 1.02f;       // > 1 : auto-oscillation possible
    static constexpr float kMinFreqHz = 20.0f;
    static constexpr float kFreezeSeconds = 0.30f;     // durée visée de la boucle capturée
    static constexpr float kFreezeFadeSeconds = 0.02f; // fondu de raccord de la boucle

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

        // Enregistreur de la sortie humide et mémoire de la boucle gelée
        const int need = (int) (fs * (kFreezeSeconds + kFreezeFadeSeconds)) + 64;
        int n = 1;
        while (n < need) n <<= 1;
        rec.assign ((size_t) n, 0.0f);
        recMask = n - 1;
        fzBuf.assign ((size_t) need, 0.0f);
        fzSlew = 1.0f - std::exp (-1.0f / (0.015f * fs));

        update (ResonatorParams {});
        reset();
    }
    void reset()
    {
        delay.reset(); inLp.reset(); loop.reset(); dc.reset(); bw.reset(); jitterLp.reset(); lastLoop = 0.0f;
        std::fill (rec.begin(), rec.end(), 0.0f);
        recW = 0; frozen = false; fzMix = 0.0f; fzPos = 0;
    }

    void update (const ResonatorParams& p)
    {
        const float fb = std::clamp (p.feedback, 0.0f, 1.0f);
        const float f0 = 440.0f * std::exp2 ((p.semitones - 69.0f) / 12.0f);
        periodSamples = fs / f0;
        loopGain = fb * kMaxLoopGain;
        inGain = 1.0f - 0.8f * fb;
        loop.setCutoff (p.loopLpHz, fs);
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
        const float bbdAmt = std::clamp (p.bbd, 0.0f, 1.0f);
        const float R = 770.0f * std::exp2 (-5.5f * bbdAmt);
        const float bwHz = 0.2f * R * f0;
        bwOn = bbdAmt > 0.001f && bwHz < 0.45f * fs;
        float pd = loop.phaseDelay (f0, fs);
        if (bwOn) { bw.setLowpass (bwHz, fs); pd += bw.phaseDelay (f0, fs); }
        else      { bw.reset(); }
        const float nz = std::clamp (p.noise, 0.0f, 1.0f);
        noiseAmp = 0.0025f * nz * nz * p.noiseGain;      // souffle injecté dans la ligne BBD
        jitterAmt = 0.00025f * bbdAmt * p.jitterGain;    // grain d'horloge

        // Couplage entre canaux : le chemin croisé a un échantillon de retard en plus, on le compense
        crossAmt = std::clamp (p.cross, 0.0f, 1.0f);
        if (crossAmt > 0.0f)
        {
            const float w = 2.0f * kPi * f0 / fs;
            const std::complex<float> m = std::complex<float> (1.0f - crossAmt, 0.0f) + crossAmt * std::polar (1.0f, -w);
            pd += -std::arg (m) / w;
        }

        // Les filtres de la boucle retardent la phase : on les compense pour rester juste
        delaySamples = std::clamp (fs / f0 - pd, 2.0f, maxDelay);

        // Freeze : front montant = capture, front descendant = retour au signal direct
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
        float v = loop.process (raw);
        if (bwOn) v = bw.process (v);
        lastLoop = v;
        const float vm = (1.0f - crossAmt) * v + crossAmt * crossV;
        const float y = in + std::tanh (loopGain * vm);  // saturation douce dans la boucle
        delay.push (y);

        // Sortie humide = sortie de la ligne (v). On l'enregistre en continu pour Freeze.
        rec[(size_t) recW] = v;
        recW = (recW + 1) & recMask;
        float out = v;
        if (frozen || fzMix > 0.0f)
        {
            out += (loopSample() - v) * fzMix;
            fzMix += ((frozen ? 1.0f : 0.0f) - fzMix) * fzSlew;
            if (! frozen && fzMix < 1.0e-4f) fzMix = 0.0f;
        }
        return out;
    }
    float loopOut() const { return lastLoop; }

private:
    // Copie les derniers échantillons humides sur un nombre ENTIER de périodes du résonateur
    // (l'accord est conservé), avec une zone de raccord pour boucler sans clic.
    void captureLoop()
    {
        const float period = std::max (periodSamples, 2.0f);
        const int maxLoop = (int) (fs * kFreezeSeconds);
        const int k = std::max (1, (int) std::floor ((float) maxLoop / period));
        const int len = std::clamp ((int) std::lround ((float) k * period), 32, maxLoop + 1);
        const int fade = std::min ((int) (fs * kFreezeFadeSeconds), len / 2);
        const int total = len + fade;
        for (int n = 0; n < total; ++n)
            fzBuf[(size_t) n] = rec[(size_t) ((recW - total + n) & recMask)];
        fzLen = len; fzFade = fade; fzPos = 0;
        frozen = true;
    }
    // Lecture de la boucle : dans la zone de raccord, on fond vers les échantillons qui PRÉCÈDENT le début
    float loopSample()
    {
        const int j = fzPos;
        float smp = fzBuf[(size_t) (fzFade + j)];
        if (j >= fzLen - fzFade)
        {
            const float w = (float) (j - (fzLen - fzFade) + 1) / (float) fzFade;
            smp = smp * (1.0f - w) + fzBuf[(size_t) (j - (fzLen - fzFade))] * w;
        }
        fzPos = (j + 1 >= fzLen) ? 0 : j + 1;
        return smp;
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

    float fs = 48000.0f, maxDelay = 1000.0f, delaySamples = 100.0f, periodSamples = 100.0f;
    float loopGain = 0.0f, inGain = 1.0f, crossAmt = 0.0f, lastLoop = 0.0f;
    float driveGain = 1.0f, driveComp = 1.0f, driveMix = 0.0f;
    int curve = 0;
    bool bwOn = false;
    float noiseAmp = 0.0f, jitterAmt = 0.0f, jitterNorm = 1.0f;
    std::uint32_t rng = 1u;
    DelayLine delay;
    Biquad inLp, bw;
    OnePole loop, jitterLp;
    DcBlocker dc;

    // Freeze
    std::vector<float> rec, fzBuf;
    int recMask = 0, recW = 0, fzLen = 0, fzFade = 0, fzPos = 0;
    bool frozen = false;
    float fzMix = 0.0f, fzSlew = 0.001f;
};
} // namespace bbdres
