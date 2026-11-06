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
        p.drive = dr; p.curve = cv; p.bbd = bb;
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
 
// Justesse : position du pic de résonance du fondamental (balayage spectral fin de la réponse impulsionnelle)
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
 
    const double f0 = 440.0 * std::pow (2.0, ((double) note - 69.0) / 12.0);
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
 
// Niveau RMS de la sortie pour une entrée silencieuse
static float silentRms (float bbd)
{
    Resonator r; r.prepare (48000.0f);
    ResonatorParams p; p.feedback = 0.9f; p.bbd = bbd;
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
 
    const float notes[] = { 36.0f, 48.0f, 60.0f, 72.0f, 84.0f };
    const float bbds[] = { 0.0f, 0.5f, 1.0f };
    for (float bb : bbds) for (float n : notes)
    {
        const float c = measureCents (48000.0f, n, bb);
        std::printf ("bbd %.1f  note %.0f : ecart d'accord = %+.2f cents\n", bb, n, c);
        check (std::fabs (c) < 1.0f, "accord hors tolerance (1 cent)");
    }
 
    const float rms0 = silentRms (0.0f), rms1 = silentRms (1.0f);
    std::printf ("silence : rms bbd=0 -> %.6f ; bbd=1 -> %.6f\n", rms0, rms1);
    check (rms0 == 0.0f, "bruit present alors que BBD = 0");
    check (rms1 > 0.0f && rms1 < 0.1f, "bruit BBD hors plage");
 
    const float e0 = highBandEnergy (0.0f), e1 = highBandEnergy (1.0f);
    std::printf ("energie aigue, note 36 : bbd=0 -> %.3f ; bbd=1 -> %.3f\n", e0, e1);
    check (e1 < 0.5f * e0, "le BBD n'assombrit pas les graves");
 
    std::printf (failures == 0 ? "TOUS LES TESTS PASSENT\n" : "%d ECHEC(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
