// Tests du coeur DSP : stabilité du feedback et justesse de l'accord.
#include "../Source/Dsp/Resonator.h"
#include <cstdio>
#include <cstdlib>
 
using bbdres::Resonator;
 
static int failures = 0;
static void check (bool ok, const char* what)
{
    if (! ok) { std::printf ("ECHEC : %s\n", what); ++failures; }
}
 
static float rnd() { return 2.0f * (float) std::rand() / (float) RAND_MAX - 1.0f; }
 
// Stabilité : bruit fort, silence, sinus, pour des réglages extrêmes
static void testStability (float fs)
{
    const float fbs[] = { 0.0f, 0.5f, 0.95f, 1.0f };
    const float notes[] = { 24.0f, 48.0f, 72.0f, 96.5f };
    const float lps[] = { 200.0f, 20000.0f };
    float worst = 0.0f;
    bool finite = true;
 
    for (float fb : fbs) for (float n : notes) for (float a : lps) for (float b : lps)
    {
        Resonator r; r.prepare (fs); r.update (n, fb, a, b); r.reset();
        const int total = (int) (fs * 4.0f);
        for (int i = 0; i < total; ++i)
        {
            float x = 0.0f;
            if (i < (int) (fs * 1.5f)) x = rnd();                                   // bruit plein niveau
            else if (i > (int) (fs * 3.0f)) x = 0.8f * std::sin (0.05f * (float) i); // sinus
            const float y = r.process (x);
            if (! std::isfinite (y)) finite = false;
            worst = std::max (worst, std::fabs (y));
        }
    }
    std::printf ("fs=%.0f  crete maximale = %.2f\n", fs, worst);
    check (finite, "valeur non finie (NaN/Inf)");
    check (worst < 8.0f, "sortie trop forte");
}
 
// Justesse : mesure la période de l'écho par autocorrélation
static float measureCents (float fs, float note)
{
    Resonator r; r.prepare (fs); r.update (note, 0.9f, 20000.0f, 12000.0f); r.reset();
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
    const double measured = (double) best + frac;
    return (float) (1200.0 * std::log2 ((double) expected / measured));
}
 
int main()
{
    testStability (44100.0f);
    testStability (48000.0f);
    testStability (192000.0f);
 
    const float notes[] = { 36.0f, 48.0f, 60.0f, 72.0f, 84.0f };
    for (float n : notes)
    {
        const float c = measureCents (48000.0f, n);
        std::printf ("note %.0f : ecart d'accord = %+.2f cents\n", n, c);
        check (std::fabs (c) < 5.0f, "accord hors tolerance (5 cents)");
    }
 
    std::printf (failures == 0 ? "TOUS LES TESTS PASSENT\n" : "%d ECHEC(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
 
