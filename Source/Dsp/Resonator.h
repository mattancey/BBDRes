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

// Mémoire du looper (Freeze) : de 30 ms à 4 s
inline constexpr float kHoldMinSeconds = 0.03f;
inline constexpr float kHoldMaxSeconds = 4.0f;

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
    float freeze = 0.0f;       // > 0.5 : le looper capture le son et le rejoue
    float holdSeconds = 0.35f; // longueur de la boucle, en secondes (déjà quantifiée si besoin)
    float holdMix = 1.0f;      // 0..1 : boucle contre son d'origine
    float holdSpeedSemis = 0.0f; // petite variation de vitesse de lecture (dérive de la mémoire)
    float noiseGain = 1.0f;    // réglage interne : multiplicateur du souffle
    float jitterGain = 1.0f;   // réglage interne : multiplicateur du jitter d'horloge
};

// Résonateur : filtre en peigne à feedback positif, accordé par le temps de retard.
// La sortie humide est prise à la SORTIE DE LA LIGNE BBD (après le souffle et les filtres de boucle) :
// le BBD colore donc tout le signal humide, et le signal sec revient par Mix.
//
// Freeze = looper à un seul lecteur. La sortie brute de la ligne est enregistrée en continu ; au gel,
// les 4 dernières secondes sont copiées. La boucle est la fin de cette copie, et sa longueur se règle
// en temps réel (jusqu'à 4 s). Le raccord est traité de deux façons :
//  - la fin de boucle est choisie par CORRÉLATION : on cherche, sur 25 ms, le point de coupe où la fin
//    ressemble le plus à ce qui précède le début (la phase des résonances se raccorde) ;
//  - un fondu enchaîné court (12 ms) relie la fin au début. Tout changement de longueur en temps réel
//    est lui aussi fondu entre l'ancienne et la nouvelle boucle.
// La boucle repasse en direct par le filtre de boucle et le BBD, puis se mélange au son d'origine.
class Resonator
{
public:
    static constexpr float kMaxLoopGain = 1.02f;       // > 1 : auto-oscillation possible
    static constexpr float kMinFreqHz = 20.0f;
    static constexpr float kFadeSeconds = 0.012f;      // fondu de raccord
    static constexpr float kSearchSeconds = 0.025f;    // fenêtre de recherche de la corrélation
    static constexpr float kLenChangeSeconds = 0.02f;  // intervalle minimal entre deux changements de longueur

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

        // Enregistreur continu et copie gelée : mémoire maximale + raccord + recherche
        fzCap = (int) (fs * (kHoldMaxSeconds + kFadeSeconds + kSearchSeconds + 0.05f));
        int n = 1;
        while (n < fzCap) n <<= 1;
        rec.assign ((size_t) n, 0.0f);
        recMask = n - 1;
        fzBuf.assign ((size_t) fzCap, 0.0f);
        fadeSamples = std::max (8, (int) (kFadeSeconds * fs));
        searchSamples = (int) (kSearchSeconds * fs);
        fzSlew = 1.0f - std::exp (-1.0f / (0.03f * fs));

        update (ResonatorParams {});
        reset();
    }
    void reset()
    {
        delay.reset(); inLp.reset(); loop.reset(); dc.reset(); bw.reset(); jitterLp.reset(); lastLoop = 0.0f;
        loopF.reset(); bwF.reset();
        std::fill (rec.begin(), rec.end(), 0.0f);
        recW = 0; frozen = false; fzMix = 0.0f; xfLeft = 0; sinceLenChange = 0;
    }

    void update (const ResonatorParams& p)
    {
        const float fb = std::clamp (p.feedback, 0.0f, 1.0f);
        const float f0 = 440.0f * std::exp2 ((p.semitones - 69.0f) / 12.0f);
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
        // Mêmes coefficients pour le chemin gelé ; son ÉTAT (z1, z2) ne doit JAMAIS être touché ici :
        // update() est appelé toutes les 16 échantillons et l'effacer ferait un saut à chaque appel.
        bwF.b0 = bw.b0; bwF.b1 = bw.b1; bwF.b2 = bw.b2; bwF.a1 = bw.a1; bwF.a2 = bw.a2;
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

        // Looper
        holdMix = std::clamp (p.holdMix, 0.0f, 1.0f);
        loopSpeed = std::exp2 (std::clamp (p.holdSpeedSemis, -2.0f, 2.0f) / 12.0f);
        targetLen = (int) std::lround (std::clamp (p.holdSeconds, kHoldMinSeconds, kHoldMaxSeconds) * fs);
        const bool wantFrozen = p.freeze > 0.5f;
        if (wantFrozen && ! frozen)
            captureLoop();
        else if (! wantFrozen)
            frozen = false;
        else if (sinceLenChange >= (int) (kLenChangeSeconds * fs)
                 && std::abs (targetLen - cur.L) > std::max (8, cur.L / 100))
        {
            applyLength (targetLen, true);          // longueur modifiée en temps réel
            sinceLenChange = 0;
        }
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
        rec[(size_t) recW] = raw;                 // enregistrement continu pour le looper
        recW = (recW + 1) & recMask;

        float v = loop.process (raw);
        v += (bw.process (v) - v) * bwMix;
        lastLoop = v;
        const float vm = (1.0f - crossAmt) * v + crossAmt * crossV;
        const float y = in + std::tanh (loopGain * vm);  // saturation douce dans la boucle
        delay.push (y);

        // Sortie humide = sortie de la ligne (v), mélangée à la boucle qui repasse par les mêmes filtres
        float out = v;
        if (frozen || fzMix > 0.0f)
        {
            float vf = loopF.process (loopSample());
            vf += (bwF.process (vf) - vf) * bwMix;
            out += (vf - v) * fzMix;
            fzMix += ((frozen ? holdMix : 0.0f) - fzMix) * fzSlew;
            if (! frozen && fzMix < 1.0e-4f) fzMix = 0.0f;
            ++sinceLenChange;
        }
        return out;
    }
    float loopOut() const { return lastLoop; }
    int loopLength() const { return cur.L; }   // longueur de la boucle en cours (échantillons)

private:
    // Une « tête » de lecture : position p dans la fenêtre [S, E), longueur L, fondu de raccord X
    struct Head { float p = 0.0f; int S = 0, E = 0, L = 1, X = 1; };

    // Interpolation cubique (Hermite) dans la copie gelée
    float frozenAt (float x) const
    {
        const int i0 = (int) x;
        const float f = x - (float) i0;
        const int hi = fzCap - 1;
        const float ym1 = fzBuf[(size_t) std::clamp (i0 - 1, 0, hi)], y0 = fzBuf[(size_t) std::clamp (i0, 0, hi)];
        const float y1 = fzBuf[(size_t) std::clamp (i0 + 1, 0, hi)], y2 = fzBuf[(size_t) std::clamp (i0 + 2, 0, hi)];
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

    // Dans les X derniers échantillons de la boucle, on fond vers ce qui PRÉCÈDE le début
    float readHead (const Head& h) const
    {
        float smp = frozenAt (h.p);
        const float zone = (float) (h.E - h.X);
        if (h.p >= zone)
        {
            const float wgt = (h.p - zone) / (float) h.X;
            smp = smp * (1.0f - wgt) + frozenAt (h.p - (float) h.L) * wgt;
        }
        return smp;
    }
    void advance (Head& h) const
    {
        h.p += loopSpeed;
        if (h.p >= (float) h.E) h.p -= (float) h.L;
    }

    float loopSample()
    {
        float smp = readHead (cur);
        advance (cur);
        if (xfLeft > 0)   // fondu entre l'ancienne et la nouvelle boucle
        {
            const float old = readHead (prevH);
            advance (prevH);
            const float wgt = 1.0f - (float) xfLeft / (float) xfLen;
            smp = old * (1.0f - wgt) + smp * wgt;
            --xfLeft;
        }
        return smp;
    }

    // Corrélation : décalage dl (en échantillons) de la fin de boucle, entre 0 et 25 ms, qui maximise la
    // ressemblance entre la fin de boucle et ce qui précède son début. Mesure décimée (coût constant).
    int bestDelta (int len, int fade) const
    {
        const int dmax = std::min (searchSamples, fzCap - len - fade - 2);
        if (dmax <= 0) return 0;
        const int stride = std::max (1, (int) (fs / 12000.0f)), step = std::max (1, (int) (fs / 24000.0f));
        double best = -2.0; int bd = 0;
        for (int dl = 0; dl <= dmax; dl += step)
        {
            const int e = fzCap - dl;
            double sab = 0.0, saa = 0.0, sbb = 0.0;
            for (int i = 0; i < fade; i += stride)
            {
                const double a = (double) fzBuf[(size_t) (e - fade + i)], b = (double) fzBuf[(size_t) (e - len - fade + i)];
                sab += a * b; saa += a * a; sbb += b * b;
            }
            const double score = sab / std::sqrt (saa * sbb + 1.0e-20);
            if (score > best) { best = score; bd = dl; }
        }
        return bd;
    }

    // Nouvelle longueur de boucle (fin choisie par corrélation). Si la tête est hors de la nouvelle
    // fenêtre, elle est ramenée dedans ; avec fondu, l'ancienne boucle s'éteint pendant la nouvelle.
    void applyLength (int newLen, bool withFade)
    {
        const int maxLen = fzCap - fadeSamples - searchSamples - 2;
        newLen = std::clamp (newLen, 64, maxLen);
        Head nh;
        nh.L = newLen;
        nh.X = std::max (8, std::min (fadeSamples, newLen / 3));
        nh.E = fzCap - bestDelta (newLen, nh.X);
        nh.S = nh.E - newLen;
        float pos = cur.p;
        if (pos < (float) nh.S || pos >= (float) nh.E)
        {
            float q = std::fmod (pos - (float) nh.S, (float) newLen);
            if (q < 0.0f) q += (float) newLen;
            pos = (float) nh.S + q;
        }
        nh.p = pos;
        if (withFade) { prevH = cur; xfLen = fadeSamples; xfLeft = xfLen; }
        cur = nh;
    }

    // Capture : copie des dernières secondes, puis boucle de la longueur demandée, lue depuis son début
    void captureLoop()
    {
        for (int n = 0; n < fzCap; ++n)
            fzBuf[(size_t) n] = rec[(size_t) ((recW - fzCap + n) & recMask)];
        cur = Head {};
        applyLength (targetLen, false);
        cur.p = (float) cur.S;
        xfLeft = 0;
        loopF.reset(); bwF.reset();
        sinceLenChange = 0;
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

    float fs = 48000.0f, maxDelay = 1000.0f, delaySamples = 100.0f;
    float loopGain = 0.0f, inGain = 1.0f, crossAmt = 0.0f, lastLoop = 0.0f, bwMix = 0.0f;
    float driveGain = 1.0f, driveComp = 1.0f, driveMix = 0.0f;
    int curve = 0;
    float noiseAmp = 0.0f, jitterAmt = 0.0f, jitterNorm = 1.0f;
    std::uint32_t rng = 1u;
    DelayLine delay;
    Biquad inLp, bw, bwF;
    OnePole loop, loopF, jitterLp;
    DcBlocker dc;

    // Looper
    std::vector<float> rec, fzBuf;
    int recMask = 0, recW = 0, fzCap = 0, fadeSamples = 8, searchSamples = 0;
    bool frozen = false;
    float fzMix = 0.0f, fzSlew = 0.001f, holdMix = 1.0f, loopSpeed = 1.0f;
    int targetLen = 16000, sinceLenChange = 0, xfLeft = 0, xfLen = 1;
    Head cur, prevH;
};
} // namespace bbdres
