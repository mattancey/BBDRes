// Tests du coeur DSP : stabilité, justesse de l'accord, comportement du caractère BBD.
#include "../Source/Dsp/Resonator.h"
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
 
// Justesse : mesure la période de l'écho par autocorrélation
static float measureCents (float fs, float note, float bbd)
{
    Resonator r; r.prepare (fs);
    ResonatorParams p; p.semitones = note; p.feedback = 0.9f; p.inputLpHz = 20000.0f;
    p.loopLpHz = 12000.0f; p.bbd = bbd;
    r.update (p); r.reset();
    const int n = (int) (fs * 0.5f);
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i) y[(size_t) i] = r.process (i == 0 ? 1.0f : 0.0f);
 
    const float f0 = 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
    const float expected = fs / f0;
    const int lo = (int) (expected * 0.8f), hi = (int) (expected * 1.2f);
    std::vector<double> ac ((size_t) hi + 2, 0.0);
    int best = lo;
    for (int lag = lo; lag <= hi + 1; ++lag)
    {
        double s = 0.0;
        for (int i = 0; i + lag < n; ++i) s += (double) y[(size_t) i] * (double) y[(size_t) (i + lag)];
        ac[(size_t) lag] = s;
        if (lag <= hi && s > ac[(size_t) best]) best = lag;
    }
    const double a = ac[(size_t) best - 1], b = ac[(size_t) best], c = ac[(size_t) best + 1];
    const double frac = 0.5 * (a - c) / (a - 2.0 * b + c);
    return (float) (1200.0 * std::log2 ((double) expected / ((double) best + frac)));
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
 
int main()
{
    testStability (44100.0f, true);
    testStability (48000.0f, false);
    testStability (192000.0f, false);
 
    const float notes[] = { 36.0f, 48.0f, 60.0f, 72.0f, 84.0f };
    const float bbds[] = { 0.0f, 0.5f, 1.0f };
    for (float bb : bbds) for (float n : notes)
    {
        const float c = measureCents (48000.0f, n, bb);
        std::printf ("bbd %.1f  note %.0f : ecart d'accord = %+.2f cents\n", bb, n, c);
        check (std::fabs (c) < 5.0f, "accord hors tolerance (5 cents)");
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
 
