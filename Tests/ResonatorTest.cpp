// Tests du coeur DSP : stabilité, justesse de l'accord, comportement du caractère BBD.
#include "../Source/Dsp/Resonator.h"
#include "../Source/Dsp/Pitch.h"
#include "../Source/Dsp/Modulation.h"
#include <cstdio>
#include <cstdlib>

using bbdres::Resonator;
using bbdres::ResonatorParams;

static int failures = 0;
static void check (bool ok, const char* what)
{
    if (! ok) { std::printf ("ECHEC : %s\n", what); ++failures; }
}

static float rnd() { return 2.0f * (float) std::rand() / (float) RAND_MAX - 1.0f; }

// Stabilité : bruit fort, silence, sinus, pour des réglages extrêmes
static void testStability (float fs, bool full)
{
    const float fbs[] = { 0.0f, 0.5f, 0.95f, 1.0f };
    const float notes[] = { 24.0f, 48.0f, 72.0f, 96.5f };
    const float lps[] = { 200.0f, 20000.0f };
    const float drives[] = { 0.0f, 1.0f };
    const float bbds[] = { 0.0f, 0.5f, 1.0f };
    float worst = 0.0f;
    bool finite = true;

    for (float fb : fbs) for (float n : notes) for (float a : lps) for (float b : lps)
    for (float dr : drives) for (int cv = 0; cv < 3; ++cv) for (float bb : bbds)
    {
        if (! full && (cv > 0 || bb == 0.5f || a != b)) continue;
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.semitones = n; p.feedback = fb; p.inputLpHz = a; p.loopLpHz = b;
        p.drive = dr; p.curve = cv; p.bbd = bb; p.noise = bb;
        r.update (p); r.reset();
        const int total = (int) (fs * 2.5f);
        for (int i = 0; i < total; ++i)
        {
            float x = 0.0f;
            if (i < (int) (fs * 1.0f)) x = rnd();                                    // bruit plein niveau
            else if (i > (int) (fs * 2.0f)) x = 0.8f * std::sin (0.05f * (float) i);  // sinus
            const float y = r.process (x);
            if (! std::isfinite (y)) finite = false;
            worst = std::max (worst, std::fabs (y));
        }
    }
    std::printf ("fs=%.0f (%s)  crete maximale = %.2f\n", fs, full ? "complet" : "reduit", worst);
    check (finite, "valeur non finie (NaN/Inf)");
    check (worst < 8.0f, "sortie trop forte");
}

// Position du pic de résonance du fondamental (balayage spectral fin), en cents par rapport à f0
static float peakCents (const std::vector<float>& y, float fs, double f0)
{
    const int n = (int) y.size();
    const int steps = 600;
    std::vector<double> mag ((size_t) steps + 1);
    int best = 0;
    for (int k = 0; k <= steps; ++k)
    {
        const double f = f0 * (0.97 + 0.06 * (double) k / (double) steps);
        const double w = 2.0 * 3.14159265358979 * f / (double) fs;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i) { re += (double) y[(size_t) i] * std::cos (w * i); im -= (double) y[(size_t) i] * std::sin (w * i); }
        mag[(size_t) k] = re * re + im * im;
        if (mag[(size_t) k] > mag[(size_t) best]) best = k;
    }
    best = std::clamp (best, 1, steps - 1);
    const double a = mag[(size_t) best - 1], b = mag[(size_t) best], c = mag[(size_t) best + 1];
    const double frac = 0.5 * (a - c) / (a - 2.0 * b + c);
    const double fMeasured = f0 * (0.97 + 0.06 * ((double) best + frac) / (double) steps);
    return (float) (1200.0 * std::log2 (fMeasured / f0));
}

// Justesse d'un résonateur seul
static float measureCents (float fs, float note, float bbd)
{
    Resonator r; r.prepare (fs);
    ResonatorParams p; p.semitones = note; p.feedback = 0.9f; p.inputLpHz = 20000.0f;
    p.loopLpHz = 12000.0f; p.bbd = bbd;
    p.noiseGain = 0.0f; p.jitterGain = 0.0f;   // on mesure l'accord, sans le souffle qui fausse la mesure
    r.update (p); r.reset();
    const int n = (int) (fs * 0.5f);
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i) y[(size_t) i] = r.process (i == 0 ? 1.0f : 0.0f);
    return peakCents (y, fs, 440.0 * std::pow (2.0, ((double) note - 69.0) / 12.0));
}

// Justesse du canal gauche d'une paire couplée (mêmes réglages des deux côtés)
static float measureCentsCoupled (float fs, float note, float cross)
{
    Resonator a, b; a.prepare (fs, 1u); b.prepare (fs, 2u);
    ResonatorParams p; p.semitones = note; p.feedback = 0.9f; p.inputLpHz = 20000.0f;
    p.loopLpHz = 12000.0f; p.cross = cross; p.noiseGain = 0.0f; p.jitterGain = 0.0f;
    a.update (p); b.update (p); a.reset(); b.reset();
    const int n = (int) (fs * 0.5f);
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const float va = a.loopOut(), vb = b.loopOut();
        const float x = i == 0 ? 1.0f : 0.0f;
        y[(size_t) i] = a.process (x, vb);
        b.process (x, va);
    }
    return peakCents (y, fs, 440.0 * std::pow (2.0, ((double) note - 69.0) / 12.0));
}

// Paire stéréo couplée, désaccordée, aux réglages extrêmes : doit rester bornée
static void testCoupledStability (float fs)
{
    float worst = 0.0f; bool finite = true;
    const float crosses[] = { 0.0f, 0.5f, 1.0f };
    const float notes[] = { 30.0f, 60.0f, 90.0f };
    for (float cr : crosses) for (float n : notes) for (float fb : { 0.9f, 1.0f })
    for (float fz : { 0.0f, 1.0f })
    {
        Resonator a, b; a.prepare (fs, 1u); b.prepare (fs, 2u);
        ResonatorParams p; p.feedback = fb; p.cross = cr; p.drive = 1.0f; p.bbd = 1.0f; p.noise = 1.0f;
        p.semitones = n - 0.25f; a.update (p);
        p.semitones = n + 0.25f; b.update (p);
        a.reset(); b.reset();
        const int total = (int) (fs * 3.0f);
        for (int i = 0; i < total; ++i)
        {
            if (i == (int) (fs * 1.5f)) { p.freeze = fz; p.semitones = n - 0.25f; a.update (p); p.semitones = n + 0.25f; b.update (p); }
            const float x = i < (int) (fs * 1.0f) ? rnd() : 0.0f;
            const float va = a.loopOut(), vb = b.loopOut();
            const float ya = a.process (x, vb), yb = b.process (x, va);
            if (! std::isfinite (ya) || ! std::isfinite (yb)) finite = false;
            worst = std::max (worst, std::max (std::fabs (ya), std::fabs (yb)));
        }
    }
    std::printf ("fs=%.0f  paire couplee : crete maximale = %.2f\n", fs, worst);
    check (finite, "paire couplee : valeur non finie");
    check (worst < 8.0f, "paire couplee : sortie trop forte");
}

// Outils d'analyse spectrale simples
static double dftPow (const std::vector<float>& y, double f, float fs)
{
    const double w = 2.0 * 3.14159265358979 * f / (double) fs; double re = 0, im = 0;
    for (size_t i = 0; i < y.size(); ++i) { re += y[i] * std::cos (w * (double) i); im -= y[i] * std::sin (w * (double) i); }
    return (re * re + im * im) / (double) y.size();
}
static double bandDb (const std::vector<float>& y, double f0, double f1, float fs)   // moyenne en dB sur une bande
{
    double s = 0; int k = 0;
    for (double f = f0; f <= f1; f += (f1 - f0) / 24.0) { s += 10.0 * std::log10 (dftPow (y, f, fs) + 1e-30); ++k; }
    return s / k;
}

// Freeze : le son figé garde son niveau, ne dépend plus de l'entrée, sans point de bouclage audible
static void testFreeze()
{
    const float fs = 48000.0f;
    auto rms = [] (const std::vector<float>& v) { double s = 0; for (float x : v) s += (double) x * x; return (float) std::sqrt (s / (double) v.size()); };
    // fluctuation du niveau : écart-type relatif de la valeur RMS par fenêtres de 20 ms
    auto flutter = [&] (const std::vector<float>& v)
    {
        const size_t w = (size_t) (0.02f * fs); std::vector<double> r;
        for (size_t i = 0; i + w <= v.size(); i += w) { double s = 0; for (size_t k = 0; k < w; ++k) s += (double) v[i + k] * v[i + k]; r.push_back (std::sqrt (s / (double) w)); }
        double m = 0; for (double x : r) m += x; m /= (double) r.size();
        double q = 0; for (double x : r) q += (x - m) * (x - m);
        return (float) (std::sqrt (q / (double) r.size()) / m);
    };

    // 1) pad : niveau, indépendance de l'entrée, stabilité, relâchement
    for (float hl : { 0.0f, 0.5f, 1.0f })
        for (float fb : { 0.3f, 0.95f })
        {
            Resonator a, b; a.prepare (fs, 1u); b.prepare (fs, 1u);   // a : l'entrée continue ; b : entrée coupée
            ResonatorParams p; p.semitones = 48.0f; p.feedback = fb; p.inputLpHz = 8000.0f; p.loopLpHz = 6000.0f; p.bbd = 0.3f; p.holdLen = hl;
            a.update (p); b.update (p); a.reset(); b.reset();
            float lp = 0.0f;
            auto pad = [&] { lp += 0.05f * (0.3f * rnd() - lp); return lp * 4.0f; };
            std::vector<float> win (48000);
            for (int i = 0; i < 48000; ++i) { const float x = pad(); a.process (x); b.process (x); }
            for (auto& v : win) { const float x = pad(); v = a.process (x); b.process (x); }
            const float before = rms (win), flutLive = flutter (win);
            p.freeze = 1.0f; a.update (p); b.update (p);
            for (int i = 0; i < 4800; ++i) { const float x = pad(); a.process (x); b.process (0.0f); }
            std::vector<float> wa (96000), wb (96000);
            for (size_t i = 0; i < wa.size(); ++i) { wa[i] = a.process (pad()); wb[i] = b.process (0.0f); }
            const float early = rms (wa), earlyB = rms (wb), flutFrozen = flutter (wa);
            for (int i = 0; i < 48000 * 4; ++i) { a.process (pad()); b.process (0.0f); }
            for (size_t i = 0; i < wa.size(); ++i) wa[i] = a.process (pad());
            const float late = rms (wa);
            const float dEarly = 20.0f * std::log10 (early / before), dDrift = 20.0f * std::log10 (late / early);
            const float dIndep = 20.0f * std::log10 (earlyB / early);
            std::printf ("freeze mem=%.1f fb=%.2f : niveau %+.1f dB ; derive %+.2f dB ; indep. entree %+.2f dB ; fluctuation %.2f (direct %.2f)\n",
                         hl, fb, dEarly, dDrift, dIndep, flutFrozen, flutLive);
            check (std::fabs (dEarly) < 2.5f, "freeze : niveau different de ce qu'on entendait");
            check (std::fabs (dDrift) < 0.3f, "freeze : le niveau derive");
            check (std::fabs (dIndep) < 0.3f, "freeze : la boucle depend encore de l'entree");
            check (flutFrozen < 1.3f * flutLive + 0.05f, "freeze : pulsation de niveau audible (point de bouclage)");
            p.freeze = 0.0f; a.update (p);
            bool finite = true;
            for (int i = 0; i < 48000; ++i) if (! std::isfinite (a.process (pad()))) finite = false;
            check (finite, "freeze : valeur non finie au relachement");
        }

    // 2) signal périodique : aucun saut au raccord, niveau constant
    for (float hl : { 0.0f, 0.6f, 1.0f })
    {
        const double f0 = 440.0 * std::pow (2.0, (48.0 - 69.0) / 12.0);
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.semitones = 48.0f; p.feedback = 0.9f; p.inputLpHz = 8000.0f; p.loopLpHz = 6000.0f; p.holdLen = hl;
        r.update (p); r.reset();
        auto tone = [&] (int i) { return (float) (0.3 * std::sin (2.0 * 3.14159265358979 * f0 * i / (double) fs)); };
        int n = 0; float prev = 0.0f, maxLive = 0.0f, maxFrozen = 0.0f;
        for (int i = 0; i < 96000; ++i) { const float y = r.process (tone (n++)); if (i > 48000) maxLive = std::max (maxLive, std::fabs (y - prev)); prev = y; }
        p.freeze = 1.0f; r.update (p);
        std::vector<float> fr (96000);
        for (int i = 0; i < 4800; ++i) prev = r.process (tone (n++));
        for (size_t i = 0; i < fr.size(); ++i) { const float y = r.process (tone (n++)); fr[i] = y; maxFrozen = std::max (maxFrozen, std::fabs (y - prev)); prev = y; }
        const float fl = flutter (fr);
        std::printf ("freeze periodique mem=%.1f : saut max en direct %.4f ; gele %.4f ; fluctuation %.3f\n", hl, maxLive, maxFrozen, fl);
        check (maxFrozen < 1.3f * maxLive, "freeze : saut au raccord de la boucle");
        check (fl < 0.05f, "freeze : le niveau pulse sur un signal periodique");
    }

    // 3) le son figé se joue : Tune +12 demi-tons = une octave plus haut
    {
        const double f0 = 440.0 * std::pow (2.0, (48.0 - 69.0) / 12.0);
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.semitones = 48.0f; p.feedback = 0.9f; p.inputLpHz = 8000.0f; p.loopLpHz = 12000.0f; p.holdLen = 0.4f;
        r.update (p); r.reset();
        auto tone = [&] (int i) { return (float) (0.3 * std::sin (2.0 * 3.14159265358979 * f0 * i / (double) fs)); };
        int n = 0;
        for (int i = 0; i < 96000; ++i) r.process (tone (n++));
        p.freeze = 1.0f; r.update (p);
        for (int i = 0; i < 9600; ++i) r.process (0.0f);
        p.semitones = 60.0f; r.update (p);
        for (int i = 0; i < 4800; ++i) r.process (0.0f);
        std::vector<float> y (24000);
        for (auto& v : y) v = r.process (0.0f);
        const double pLow = dftPow (y, f0, fs), pHigh = dftPow (y, 2.0 * f0, fs);
        std::printf ("freeze joue : puissance a f0 = %.5f ; a 2 x f0 = %.5f\n", pLow, pHigh);
        check (pHigh > 10.0 * pLow, "freeze : la hauteur ne suit pas Tune");
    }
}

// Pas de clic quand le BBD quitte zéro (le filtre ne doit pas s'enclencher d'un coup)
static void testBbdClick()
{
    const float fs = 48000.0f;
    for (float note : { 24.0f, 36.0f, 48.0f })
    {
        const float w0 = 2.0f * 3.14159265f * 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.semitones = note; p.feedback = 0.7f; p.inputLpHz = 20000.0f; p.loopLpHz = 12000.0f; p.bbd = 0.0f;
        p.jitterGain = 0.0f; p.noiseGain = 0.0f;
        r.update (p); r.reset();
        float prev = 0.0f, base = 0.0f, worst = 0.0f;
        for (int i = 0; i < 48000; ++i)
        {
            if (i % 16 == 0) { p.bbd = (i >= 24000) ? 0.01f : 0.0f; r.update (p); }   // marche franche 0 -> 1 %
            const float y = r.process (0.3f * std::sin (w0 * (float) i / fs));
            const float d = std::fabs (y - prev); prev = y;
            if (i > 12000 && i < 24000) base = std::max (base, d);
            if (i >= 24000) worst = std::max (worst, d);
        }
        std::printf ("clic BBD note %.0f : saut max apres marche 0 -> 1 %% = %.2f x la reference\n", note, worst / base);
        check (worst < 1.5f * base, "BBD : clic au debut de la course");
    }
}

// Souffle traité par la ligne BBD, et BBD audible sur le signal humide
static void testNoiseAndBbd()
{
    const float fs = 48000.0f;
    // 1) souffle : plus sombre que du bruit blanc quand le BBD monte
    auto tilt = [&] (float bbd, float& mid, float& hi)
    {
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.feedback = 0.7f; p.bbd = bbd; p.noise = 1.0f; p.loopLpHz = 6000.0f; p.semitones = 48.0f;
        r.update (p); r.reset();
        std::vector<float> y (24000);
        for (int i = 0; i < 48000; ++i) r.process (0.0f);
        for (auto& v : y) v = r.process (0.0f);
        const double lo = bandDb (y, 100, 1000, fs);
        mid = (float) (bandDb (y, 1000, 4000, fs) - lo); hi = (float) (bandDb (y, 4000, 12000, fs) - lo);
    };
    float m0, h0, m6, h6;
    tilt (0.0f, m0, h0); tilt (0.6f, m6, h6);
    std::printf ("souffle : BBD 0 %% -> 1-4k %+.1f dB, 4-12k %+.1f dB ; BBD 60 %% -> 1-4k %+.1f dB, 4-12k %+.1f dB\n", m0, h0, m6, h6);
    check (h0 < -2.0f, "souffle : non filtre par la boucle");
    check (h6 < h0 - 10.0f, "souffle : le BBD ne l'assombrit pas");

    // 2) le BBD doit changer nettement le signal humide, aux reglages par defaut
    auto resp = [&] (float bbd)
    {
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.feedback = 0.7f; p.loopLpHz = 6000.0f; p.inputLpHz = 20000.0f; p.semitones = 48.0f; p.bbd = bbd;
        p.noiseGain = 0.0f; p.jitterGain = 0.0f;
        r.update (p); r.reset();
        std::vector<float> y (48000);
        for (size_t i = 0; i < y.size(); ++i) y[i] = r.process (i == 0 ? 1.0f : 0.0f);
        return y;
    };
    const auto ref = resp (0.0f), y3 = resp (0.3f), y6 = resp (0.6f), y10 = resp (1.0f);
    const double d3 = bandDb (y3, 4000, 8000, fs) - bandDb (ref, 4000, 8000, fs);
    const double d6 = bandDb (y6, 2000, 4000, fs) - bandDb (ref, 2000, 4000, fs);
    const double d10 = bandDb (y10, 1000, 2000, fs) - bandDb (ref, 1000, 2000, fs);
    std::printf ("BBD : 30 %% -> 4-8k %+.1f dB ; 60 %% -> 2-4k %+.1f dB ; 100 %% -> 1-2k %+.1f dB\n", d3, d6, d10);
    check (d3 < -2.0f, "BBD 30 % : pas audible");
    check (d6 < -5.0f, "BBD 60 % : pas assez audible");
    check (d10 < -10.0f, "BBD 100 % : pas assez marque");
}

// Niveau RMS de la sortie pour une entrée silencieuse
static float silentRms (float noise)
{
    Resonator r; r.prepare (48000.0f);
    ResonatorParams p; p.feedback = 0.9f; p.bbd = 1.0f; p.noise = noise;
    r.update (p); r.reset();
    double s = 0.0; const int n = 48000;
    for (int i = 0; i < n; ++i) { const float y = r.process (0.0f); s += (double) y * y; }
    return (float) std::sqrt (s / n);
}

// Bande passante : à BBD maximal, une note grave doit être plus sombre qu'à BBD nul
static float highBandEnergy (float bbd)
{
    Resonator r; r.prepare (48000.0f);
    ResonatorParams p; p.semitones = 36.0f; p.feedback = 0.9f; p.inputLpHz = 20000.0f;
    p.loopLpHz = 20000.0f; p.bbd = bbd;
    r.update (p); r.reset();
    double e = 0.0; float prev = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = r.process (i % 4000 == 0 ? 1.0f : 0.0f);
        const float d = y - prev; prev = y;   // différence = passe-haut grossier
        e += (double) d * d;
    }
    return (float) e;
}

// Quantification : le résultat est dans la gamme et c'est bien la note la plus proche
static void testQuantizer()
{
    check (bbdres::quantizeToScale (61.2f, 0, bbdres::kScales[1].mask) == 62.0f, "Do majeur : 61.2 -> 62");
    check (bbdres::quantizeToScale (66.5f, 0, bbdres::kScales[1].mask) == 67.0f, "Do majeur : 66.5 -> 67");
    check (bbdres::quantizeToScale (61.0f, 0, bbdres::kScales[0].mask) == 61.0f, "chromatique : inchange");
    check (bbdres::quantizeToScale (62.0f, 2, bbdres::kScales[3].mask) == 62.0f, "Re dorien : la tonique reste");
    bool allIn = true, nearest = true;
    for (int sc = 0; sc < bbdres::kNumScales; ++sc)
        for (int root = 0; root < 12; ++root)
            for (int i = 0; i < 400; ++i)
            {
                const float n = 24.0f + 72.0f * (float) i / 400.0f;
                const float q = bbdres::quantizeToScale (n, root, bbdres::kScales[sc].mask);
                const int degree = ((((int) std::lround (q) - root) % 12) + 12) % 12;
                if (! ((bbdres::kScales[sc].mask >> degree) & 1u)) allIn = false;
                for (int k = -12; k <= 12; ++k)
                {
                    const float c = std::floor (n) + (float) k;
                    const int dg = ((((int) c - root) % 12) + 12) % 12;
                    if (((bbdres::kScales[sc].mask >> dg) & 1u) && std::fabs (n - c) < std::fabs (n - q) - 1.0e-4f)
                        nearest = false;
                }
            }
    check (allIn, "quantification : note hors gamme");
    check (nearest, "quantification : ce n'est pas la note la plus proche");
    std::printf ("quantification : %d gammes x 12 toniques verifiees\n", bbdres::kNumScales);
}

// LFO : bornes, formes, enveloppe
static void testModulation()
{
    for (int shape = 0; shape < 6; ++shape)
    {
        bbdres::Lfo l; l.prepare (3000.0f, 7u); l.setRateHz (2.0f);
        float lo = 9.0f, hi = -9.0f; bool ok = true;
        for (int i = 0; i < 3000 * 20; ++i)
        {
            const float v = l.next (shape);
            if (! std::isfinite (v)) ok = false;
            lo = std::min (lo, v); hi = std::max (hi, v);
        }
        check (ok && lo >= -1.0001f && hi <= 1.0001f, "LFO hors des bornes -1..+1");
        std::printf ("LFO forme %d : min %.3f max %.3f\n", shape, lo, hi);
        if (shape == 0) check (lo < -0.99f && hi > 0.99f, "sinus : amplitude incomplete");
    }
    // Synchronisation : phase imposée par la position de l'hôte
    check (std::fabs (bbdres::syncedPhase (4.0, 4.0f)) < 1.0e-6f, "sync : debut de mesure");
    check (std::fabs (bbdres::syncedPhase (5.0, 4.0f) - 0.25f) < 1.0e-6f, "sync : un quart de mesure");
    check (bbdres::syncedPhase (-1.0, 4.0f) >= 0.0f && bbdres::syncedPhase (-1.0, 4.0f) < 1.0f, "sync : position negative");
    {
        bbdres::Lfo l; l.prepare (3000.0f, 5u);
        const float a = l.nextAt (0, 0.25f);    // sinus a un quart de cycle = 1
        const float b = l.nextAt (0, 0.75f);    // trois quarts = -1
        check (std::fabs (a - 1.0f) < 1.0e-4f && std::fabs (b + 1.0f) < 1.0e-4f, "sync : forme sinus calee");
        const float r1 = l.nextAt (4, 0.9f), r2 = l.nextAt (4, 0.95f), r3 = l.nextAt (4, 0.05f);
        check (r1 == r2 && r3 != r2, "sync : le Random change au bouclage");
    }
    check (bbdres::kNumSync == 12 && bbdres::kSyncBeats[5] == 1.0f && bbdres::kSyncBeats[3] == 4.0f, "table de sync");
    bbdres::EnvFollower e; e.prepare (3000.0f);
    for (int i = 0; i < 90; ++i) e.next (1.0f);   // 30 ms
    check (e.y > 0.9f, "enveloppe : attaque trop lente");
    for (int i = 0; i < 600; ++i) e.next (0.0f);
    check (e.y < 0.5f && e.y > 0.0f, "enveloppe : relachement incorrect");
    check (std::fabs (bbdres::levelToUnit (1.0f) - 1.0f) < 1.0e-4f, "niveau 0 dB -> 1");
    check (std::fabs (bbdres::levelToUnit (0.1f) - 0.5f) < 1.0e-3f, "niveau -20 dB -> 0.5");
    check (bbdres::levelToUnit (0.001f) == 0.0f, "niveau -60 dB -> 0");
}

int main()
{
    testQuantizer();
    testModulation();
    testStability (44100.0f, true);
    testStability (48000.0f, false);
    testStability (192000.0f, false);
    testCoupledStability (44100.0f);
    testCoupledStability (96000.0f);
    testFreeze();
    testNoiseAndBbd();
    testBbdClick();
    for (float cr : { 0.25f, 0.5f, 1.0f })
        for (float n : { 36.0f, 60.0f, 84.0f })
        {
            const float c = measureCentsCoupled (48000.0f, n, cr);
            std::printf ("cross %.2f  note %.0f : ecart d'accord = %+.2f cents\n", cr, n, c);
            check (std::fabs (c) < 2.0f, "accord avec Cross hors tolerance (2 cents)");
        }

    const float notes[] = { 36.0f, 48.0f, 60.0f, 72.0f, 84.0f };
    const float bbds[] = { 0.0f, 0.5f, 1.0f };
    for (float bb : bbds) for (float n : notes)
    {
        const float c = measureCents (48000.0f, n, bb);
        std::printf ("bbd %.1f  note %.0f : ecart d'accord = %+.2f cents\n", bb, n, c);
        check (std::fabs (c) < (bb >= 1.0f ? 6.0f : 1.0f), "accord hors tolerance");
    }

    const float rms0 = silentRms (0.0f), rms1 = silentRms (1.0f);
    std::printf ("silence (BBD 100 %%) : rms noise=0 -> %.6f ; noise=1 -> %.6f\n", rms0, rms1);
    check (rms0 == 0.0f, "souffle present alors que Noise = 0");
    check (rms1 > 0.0f && rms1 < 0.05f, "souffle Noise hors plage");

    const float e0 = highBandEnergy (0.0f), e1 = highBandEnergy (1.0f);
    std::printf ("energie aigue, note 36 : bbd=0 -> %.3f ; bbd=1 -> %.3f\n", e0, e1);
    check (e1 < 0.5f * e0, "le BBD n'assombrit pas les graves");

    std::printf (failures == 0 ? "TOUS LES TESTS PASSENT\n" : "%d ECHEC(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
