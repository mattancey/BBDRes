// Tests du coeur DSP : stabilité, justesse de l'accord, comportement du caractère BBD.
#include "../Source/Dsp/Resonator.h"
#include "../Source/Dsp/Pitch.h"
#include "../Source/Dsp/Modulation.h"
#include "../Source/Dsp/Response.h"
#include "../Source/Dsp/Sequencer.h"
#include <cstdio>
#include <string>
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

// Freeze = looper à un seul lecteur : longueur exacte, raccord sans clic ni creux de niveau,
// longueur modifiable en temps réel, fader de mix
static void testLooper()
{
    const float fs = 48000.0f;
    auto rms = [] (const std::vector<float>& v) { double s = 0; for (float x : v) s += (double) x * x; return (float) std::sqrt (s / (double) v.size()); };
    auto flutter = [&] (const std::vector<float>& v)   // écart-type relatif du niveau RMS par fenêtres de 20 ms
    {
        const size_t w = (size_t) (0.02f * fs); std::vector<double> r;
        for (size_t i = 0; i + w <= v.size(); i += w) { double s = 0; for (size_t k = 0; k < w; ++k) s += (double) v[i + k] * v[i + k]; r.push_back (std::sqrt (s / (double) w)); }
        double m = 0; for (double x : r) m += x; m /= (double) r.size();
        double q = 0; for (double x : r) q += (x - m) * (x - m);
        return (float) (std::sqrt (q / (double) r.size()) / m);
    };
    // Un pas du plug-in : les réglages sont mis à jour tous les 16 échantillons
    auto run = [] (Resonator& r, ResonatorParams& p, int count, auto&& input, std::vector<float>* out)
    {
        for (int i = 0; i < count; ++i)
        {
            if (i % 16 == 0) r.update (p);
            const float y = r.process (input());
            if (out) out->push_back (y);
        }
    };
    auto makePad = [] { float lp = 0.0f; return [lp]() mutable { lp += 0.05f * (0.3f * rnd() - lp); return lp * 4.0f; }; };
    auto base = [] { ResonatorParams p; p.semitones = 48.0f; p.feedback = 0.7f; p.inputLpHz = 8000.0f; p.loopLpHz = 6000.0f; p.bbd = 0.3f; return p; };
    auto silence = [] { return 0.0f; };

    // 1) longueur exacte, boucle strictement périodique, niveau conservé, indépendante de l'entrée
    {
        Resonator a, b; a.prepare (fs, 1u); b.prepare (fs, 1u);
        auto p = base(); p.holdSeconds = 0.25f;
        a.update (p); b.update (p); a.reset(); b.reset();
        auto gen = makePad();
        std::vector<float> src;                                    // même signal pour les deux résonateurs
        for (int i = 0; i < 144000 + 96000; ++i) src.push_back (gen());
        size_t k = 0; auto feed = [&] { return src[k++]; };
        k = 0; run (a, p, 96000, feed, nullptr);
        k = 0; run (b, p, 96000, feed, nullptr);
        std::vector<float> before;
        k = 96000; run (a, p, 48000, feed, &before);
        k = 96000; run (b, p, 48000, feed, nullptr);
        const float levelBefore = rms (before);
        p.freeze = 1.0f;
        k = 144000; run (a, p, 24000, feed, nullptr);              // a : l'entrée continue
        run (b, p, 24000, silence, nullptr);                       // b : entrée coupée dès le gel
        std::vector<float> ya, yb;
        run (a, p, 72000, feed, &ya);
        run (b, p, 72000, silence, &yb);
        const int len = a.loopLength();
        float maxErr = 0.0f;
        for (size_t i = 0; i + (size_t) len < ya.size() && i < 24000; ++i) maxErr = std::max (maxErr, std::fabs (ya[i + (size_t) len] - ya[i]));
        const float lvl = rms (ya), dB = 20.0f * std::log10 (lvl / levelBefore);
        float dMax = 0.0f; for (size_t i = 0; i < ya.size(); ++i) dMax = std::max (dMax, std::fabs (ya[i] - yb[i]));
        std::printf ("looper : longueur %d (attendu 12000) ; erreur de periodicite %.5f (niveau %.3f) ; niveau %+.1f dB ; ecart sans entree %.5f\n",
                     len, maxErr, lvl, dB, dMax);
        check (std::abs (len - 12000) <= 1, "looper : longueur inexacte");
        check (maxErr < 0.03f * lvl, "looper : la boucle n'est pas periodique");
        check (std::fabs (dB) < 3.0f, "looper : niveau different de ce qu'on entendait");
        check (dMax < 0.001f, "looper : la boucle depend de l'entree");
        p.freeze = 0.0f;
        bool finite = true;
        for (int i = 0; i < 48000; ++i) { if (i % 16 == 0) a.update (p); if (! std::isfinite (a.process (feed()))) finite = false; }
        check (finite, "looper : valeur non finie au relachement");
    }

    // 2) signal périodique, longueur sans rapport avec la période : la corrélation évite creux et clics
    for (float secs : { 0.2347f, 0.5111f, 1.37f })
    {
        const double f0 = 440.0 * std::pow (2.0, (48.0 - 69.0) / 12.0);
        Resonator r; r.prepare (fs);
        auto p = base(); p.feedback = 0.9f; p.holdSeconds = secs;
        r.update (p); r.reset();
        int n = 0;
        auto tone = [&] { return (float) (0.3 * std::sin (2.0 * 3.14159265358979 * f0 * n++ / (double) fs)); };
        std::vector<float> live, fr;
        run (r, p, 192000, tone, nullptr); run (r, p, 48000, tone, &live);
        p.freeze = 1.0f;
        run (r, p, 14400, tone, nullptr);
        run (r, p, (int) (fs * 3.0f * std::max (secs, 0.5f)), tone, &fr);
        float maxLive = 0.0f, maxFr = 0.0f;
        for (size_t i = 1; i < live.size(); ++i) maxLive = std::max (maxLive, std::fabs (live[i] - live[i - 1]));
        for (size_t i = 1; i < fr.size(); ++i) maxFr = std::max (maxFr, std::fabs (fr[i] - fr[i - 1]));
        const float fl = flutter (fr);
        std::printf ("looper periodique %.4f s : saut max direct %.4f ; boucle %.4f ; fluctuation %.3f\n", secs, maxLive, maxFr, fl);
        check (maxFr < 1.3f * maxLive, "looper : clic au raccord");
        check (fl < 0.06f, "looper : creux de niveau au raccord");
    }

    // 3) longueur modifiée en temps réel : fondu sans clic, nouvelle longueur exacte
    {
        Resonator r; r.prepare (fs);
        auto p = base(); p.holdSeconds = 0.4f;
        r.update (p); r.reset();
        auto pad = makePad();
        run (r, p, 144000, pad, nullptr);
        p.freeze = 1.0f;
        std::vector<float> y1, y2, y3;
        run (r, p, 48000, silence, &y1);
        float maxDiff1 = 0.0f; for (size_t i = 24000; i < y1.size(); ++i) maxDiff1 = std::max (maxDiff1, std::fabs (y1[i] - y1[i - 1]));
        p.holdSeconds = 0.15f;                                 // 400 ms -> 150 ms en cours de lecture
        run (r, p, 24000, silence, &y2);
        p.holdSeconds = 0.25f;                                 // puis 250 ms
        run (r, p, 48000, silence, &y3);
        float maxDiff2 = 0.0f; bool finite = true;
        for (float v : y2) if (! std::isfinite (v)) finite = false;
        for (size_t i = 1; i < y2.size(); ++i) maxDiff2 = std::max (maxDiff2, std::fabs (y2[i] - y2[i - 1]));
        const int len = r.loopLength();
        float maxErr = 0.0f;
        for (size_t i = 12000; i + (size_t) len < y3.size() && i < 30000; ++i) maxErr = std::max (maxErr, std::fabs (y3[i + (size_t) len] - y3[i]));
        std::printf ("longueur en temps reel : saut max avant %.4f ; pendant le changement %.4f ; longueur finale %d (attendu 12000) ; periodicite %.5f\n",
                     maxDiff1, maxDiff2, len, maxErr);
        check (finite, "looper : valeur non finie pendant le changement de longueur");
        check (maxDiff2 < 2.5f * maxDiff1, "looper : clic pendant le changement de longueur");
        check (std::abs (len - 12000) <= 1, "looper : la nouvelle longueur n'est pas appliquee");
        check (maxErr < 0.03f * rms (y3), "looper : boucle non periodique apres le changement");
    }

    // 4) fader de mix : 0 % = son d'origine exactement, 100 % = boucle seule
    {
        Resonator a, b, c; a.prepare (fs, 1u); b.prepare (fs, 1u); c.prepare (fs, 1u);
        auto p = base(); p.holdSeconds = 0.25f; p.holdMix = 0.0f;
        auto pb = base(); pb.holdSeconds = 0.25f;
        auto pc = p; pc.holdMix = 1.0f;
        a.update (p); b.update (pb); c.update (pc); a.reset(); b.reset(); c.reset();
        std::vector<float> ya, yb, yc;
        auto pad1 = makePad(); std::vector<float> src;
        for (int i = 0; i < 96000; ++i) src.push_back (pad1());
        size_t k = 0; auto feed = [&] { return src[k++ % src.size()]; };
        k = 0; run (a, p, 96000, feed, nullptr);
        k = 0; run (b, pb, 96000, feed, nullptr);
        k = 0; run (c, pc, 96000, feed, nullptr);
        p.freeze = 1.0f; pc.freeze = 1.0f;
        k = 0; run (a, p, 48000, feed, &ya);
        k = 0; run (b, pb, 48000, feed, &yb);
        k = 0; run (c, pc, 48000, feed, &yc);
        float d0 = 0.0f; for (size_t i = 0; i < ya.size(); ++i) d0 = std::max (d0, std::fabs (ya[i] - yb[i]));
        float d1 = 0.0f; for (size_t i = 24000; i < yc.size(); ++i) d1 = std::max (d1, std::fabs (yc[i] - yb[i]));
        std::printf ("mix du looper : 0 %% -> ecart avec le son d'origine %.6f ; 100 %% -> ecart %.4f\n", d0, d1);
        check (d0 < 1.0e-6f, "looper : mix 0 % different du son d'origine");
        check (d1 > 0.01f, "looper : mix 100 % identique au son d'origine");
    }

    // 5) quantification rythmique de la longueur
    check (std::fabs (bbdres::quantizeHoldSeconds (0.26f, 120.0f) - 0.25f) < 1.0e-6f, "quantize : 0.26 s a 120 BPM -> 1/8");
    check (std::fabs (bbdres::quantizeHoldSeconds (1.9f, 120.0f) - 2.0f) < 1.0e-6f, "quantize : 1.9 s a 120 BPM -> 1 mesure");
    check (std::fabs (bbdres::quantizeHoldSeconds (3.1f, 60.0f) - 3.0f) < 1.0e-6f, "quantize : 3.1 s a 60 BPM -> 3 temps");
    check (std::fabs (bbdres::quantizeHoldSeconds (0.1f, 120.0f) - 0.125f) < 1.0e-6f, "quantize : 0.1 s a 120 BPM -> 1/16");
    check (std::fabs (bbdres::quantizeHoldSeconds (10.0f, 120.0f) - 4.0f) < 1.0e-6f, "quantize : borne a 4 s");
    check (bbdres::quantizeHoldIndex (0.26f, 120.0f) == 2 && std::string (bbdres::kHoldNames[2]) == "1/8", "quantize : nom de la division");
    check (std::fabs (bbdres::holdSecondsFromSlider (0.0f) - 0.03f) < 1.0e-6f && std::fabs (bbdres::holdSecondsFromSlider (1.0f) - 4.0f) < 1.0e-4f, "curseur de longueur");
}

// Release du looper : 0,5 s linéaire quand on décoche Freeze ; entrée rapide
static void testRelease()
{
    const float fs = 48000.0f;
    Resonator a, b; a.prepare (fs, 1u); b.prepare (fs, 1u);
    ResonatorParams p; p.semitones = 48.0f; p.feedback = 0.7f; p.inputLpHz = 8000.0f; p.loopLpHz = 6000.0f; p.bbd = 0.3f; p.holdSeconds = 0.3f;
    a.update (p); b.update (p); a.reset(); b.reset();
    // même signal pour les deux : a passe par le looper, b reste en direct
    std::vector<float> src; float lp = 0.0f;
    for (int i = 0; i < 48000 * 7; ++i) { lp += 0.05f * (0.3f * rnd() - lp); src.push_back (lp * 4.0f); }
    size_t k = 0;
    auto step = [&] (ResonatorParams& pa, std::vector<float>* diff)
    {
        if (k % 16 == 0) { a.update (pa); b.update (p); }
        const float x = src[k++];
        const float ya = a.process (x), yb = b.process (x);
        if (diff) diff->push_back (ya - yb);
    };
    auto pa = p;
    for (int i = 0; i < 48000 * 3; ++i) step (pa, nullptr);
    pa.freeze = 1.0f;
    std::vector<float> engage;
    for (int i = 0; i < 48000 * 2; ++i) step (pa, &engage);
    pa.freeze = 0.0f;
    std::vector<float> rel;
    for (int i = 0; i < 48000 * 1; ++i) step (pa, &rel);
    auto rmsw = [&] (const std::vector<float>& v, float t0, float t1)
    {
        double s2 = 0; const size_t i0 = (size_t) (t0 * fs), i1 = std::min (v.size(), (size_t) (t1 * fs));
        for (size_t i = i0; i < i1; ++i) s2 += (double) v[i] * v[i];
        return (float) std::sqrt (s2 / (double) (i1 - i0));
    };
    const float steady = rmsw (engage, 1.0f, 2.0f);
    const float e100 = rmsw (engage, 0.08f, 0.10f) / steady;              // 100 ms après l'engagement
    const float r0 = rmsw (rel, 0.0f, 0.05f), r250 = rmsw (rel, 0.225f, 0.275f), r400 = rmsw (rel, 0.375f, 0.425f);
    const float rEnd = rmsw (rel, 0.55f, 0.9f);
    std::printf ("release : entree a 100 ms = %.2f du regime ; ecart direct/looper : debut %.4f, 250 ms %.2f, 400 ms %.2f, apres 0.55 s %.6f\n",
                 e100, r0, r250 / r0, r400 / r0, rEnd);
    check (e100 > 0.8f, "release : l'entree dans le looper est trop lente");
    check (r250 / r0 > 0.3f && r250 / r0 < 0.7f, "release : pas lineaire a mi-parcours");
    check (r400 / r0 < 0.45f, "release : trop lente");
    check (rEnd < 1.0e-6f, "release : le looper ne s'eteint pas en 0,5 s");
}

// Filtres du looper : passe-haut et passe-bas 12 dB, uniquement sur la boucle
static void testHoldFilter()
{
    const float fs = 48000.0f;
    std::vector<float> src;
    for (int i = 0; i < 48000 * 4; ++i) src.push_back (0.3f * rnd());           // bruit large bande
    auto capture = [&] (float hp, float lp)
    {
        Resonator r; r.prepare (fs, 1u);
        ResonatorParams p; p.semitones = 48.0f; p.feedback = 0.7f; p.inputLpHz = 20000.0f; p.loopLpHz = 20000.0f; p.bbd = 0.0f; p.holdSeconds = 0.5f;
        r.update (p); r.reset();
        for (int i = 0; i < 48000 * 2; ++i) { if (i % 16 == 0) r.update (p); r.process (src[(size_t) i]); }
        p.freeze = 1.0f; p.holdHpHz = hp; p.holdLpHz = lp;
        std::vector<float> y;
        for (int i = 0; i < 48000 * 2; ++i) { if (i % 16 == 0) r.update (p); const float v = r.process (src[(size_t) (i + 96000)]); if (i >= 48000) y.push_back (v); }
        return y;
    };
    // Mesure avec fenêtre de Hann : sans elle, la fuite spectrale des graves masque les aigus très atténués
    auto db = [&] (const std::vector<float>& y, double f0, double f1)
    {
        double acc = 0.0; int cnt = 0; const size_t n = y.size();
        for (double f = f0; f <= f1; f += (f1 - f0) / 24.0)
        {
            const double w = 2.0 * 3.14159265358979 * f / (double) fs; double re = 0, im = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) (n - 1));
                re += y[i] * win * std::cos (w * (double) i); im -= y[i] * win * std::sin (w * (double) i);
            }
            acc += 10.0 * std::log10 ((re * re + im * im) / (double) n + 1e-30); ++cnt;
        }
        return acc / cnt;
    };
    const auto open = capture (20.0f, 20000.0f);
    const auto hp = capture (1000.0f, 20000.0f), lp = capture (20.0f, 1000.0f), band = capture (500.0f, 1000.0f), closed = capture (2000.0f, 500.0f);
    const double lowHp = db (hp, 100, 200) - db (open, 100, 200), midHp = db (hp, 3000, 4000) - db (open, 3000, 4000);
    const double highLp = db (lp, 5000, 8000) - db (open, 5000, 8000), midLp = db (lp, 300, 600) - db (open, 300, 600);
    const double lowB = db (band, 100, 200) - db (open, 100, 200), highB = db (band, 5000, 8000) - db (open, 5000, 8000);
    double so = 0, sc = 0;
    for (float v : open) so += (double) v * v;
    for (float v : closed) sc += (double) v * v;
    const double closedDb = 10.0 * std::log10 (sc / so);
    std::printf ("filtres du looper : HP 1 kHz -> 100-200 Hz %+.1f dB, 3-4 kHz %+.1f dB ; LP 1 kHz -> 5-8 kHz %+.1f dB, 300-600 Hz %+.1f dB ; "
                 "bande 500-1000 -> graves %+.1f dB, aigus %+.1f dB ; HP 2k + LP 500 (croises) -> %+.1f dB\n",
                 lowHp, midHp, highLp, midLp, lowB, highB, closedDb);
    check (lowHp < -18.0, "HP : les graves ne sont pas assez coupes");
    check (std::fabs (midHp) < 1.5, "HP : les mediums sont alteres");
    check (highLp < -18.0, "LP : les aigus ne sont pas assez coupes");
    check (std::fabs (midLp) < 1.5, "LP : les mediums sont alteres");
    check (lowB < -12.0 && highB < -12.0, "bande : graves et aigus doivent etre coupes");
    check (closedDb < -25.0, "filtres croises : presque rien ne doit passer");

    // le son direct n'est jamais touché : sans Freeze, les réglages des filtres ne changent rien
    Resonator a, b; a.prepare (fs, 1u); b.prepare (fs, 1u);
    ResonatorParams pa; pa.holdHpHz = 5000.0f; pa.holdLpHz = 300.0f;
    ResonatorParams pb;
    a.update (pa); b.update (pb); a.reset(); b.reset();
    float dmax = 0.0f;
    for (int i = 0; i < 48000; ++i) { if (i % 16 == 0) { a.update (pa); b.update (pb); } dmax = std::max (dmax, std::fabs (a.process (src[(size_t) i]) - b.process (src[(size_t) i]))); }
    check (dmax == 0.0f, "les filtres du looper modifient le son direct");
}

// La courbe de l'interface (modèle analytique) doit coller à la réponse mesurée du vrai résonateur
static void testResponseModel()
{
    const float fs = 48000.0f;
    struct Cfg { float note, fb, inLp, loopLp, bbd, lowHz, lowAmt; };
    double worstAll = 0.0;
    for (Cfg c : { Cfg { 48.0f, 0.7f, 8000.0f, 6000.0f, 0.3f, 80.0f, 0.0f }, Cfg { 60.0f, 0.9f, 12000.0f, 12000.0f, 0.6f, 80.0f, 0.0f },
                   Cfg { 36.0f, 0.5f, 5000.0f, 3000.0f, 0.0f, 80.0f, 0.0f }, Cfg { 48.0f, 0.8f, 12000.0f, 12000.0f, 0.2f, 200.0f, 0.7f },
                   Cfg { 55.0f, 0.9f, 12000.0f, 8000.0f, 0.0f, 300.0f, 1.0f } })
    {
        Resonator r; r.prepare (fs);
        ResonatorParams p; p.semitones = c.note; p.feedback = c.fb; p.inputLpHz = c.inLp; p.loopLpHz = c.loopLp; p.bbd = c.bbd;
        p.lowCutHz = c.lowHz; p.lowCutAmt = c.lowAmt;
        p.noiseGain = 0.0f; p.jitterGain = 0.0f;
        r.update (p); r.reset();
        const int n = 96000;
        std::vector<float> y ((size_t) n);
        for (int i = 0; i < n; ++i) y[(size_t) i] = r.process (i == 0 ? 0.001f : 0.0f);   // impulsion faible : régime linéaire

        bbdres::ResponseParams rp; rp.semitones = c.note; rp.feedback = c.fb; rp.inputLpHz = c.inLp; rp.loopLpHz = c.loopLp; rp.bbd = c.bbd; rp.lowCutHz = c.lowHz; rp.lowCutAmt = c.lowAmt; rp.fs = fs;
        bbdres::ResponseModel m; m.set (rp);
        const double f0 = (double) m.fundamentalHz();
        double worst = 0.0;
        for (double mult : { 1.0, 2.0, 3.5, 5.0, 7.5, 12.0 })
        {
            const double f = f0 * mult;
            if (f > 9000.0) continue;
            const double meas = 10.0 * std::log10 (dftPow (y, f, fs) * n + 1e-30) + 60.0;   // impulsion de 0,001
            const double model = (double) m.db ((float) f);
            worst = std::max (worst, std::fabs (meas - model));
        }
        std::printf ("courbe : note %.0f fb %.1f bbd %.1f pultec %.1f -> ecart max modele/mesure = %.2f dB\n", c.note, c.fb, c.bbd, c.lowAmt, worst);
        worstAll = std::max (worstAll, worst);
    }
    check (worstAll < 1.5, "la courbe de l'interface ne colle pas a la reponse reelle (1,5 dB)");
}

// Atténuateur de graves « Pultec » de la boucle : transparent à 0, passe-haut 6 dB à 100 %, accord conservé
static void testPultec()
{
    const float fs = 48000.0f;
    std::vector<float> src;
    for (int i = 0; i < 48000 * 3; ++i) src.push_back (0.3f * rnd());
    auto render = [&] (float lowHz, float amt)
    {
        Resonator r; r.prepare (fs, 1u);
        ResonatorParams p; p.semitones = 48.0f; p.feedback = 0.7f; p.inputLpHz = 20000.0f; p.loopLpHz = 20000.0f; p.bbd = 0.0f;
        p.lowCutHz = lowHz; p.lowCutAmt = amt;
        r.update (p); r.reset();
        std::vector<float> y;
        for (int i = 0; i < 48000 * 3; ++i) { if (i % 16 == 0) r.update (p); const float v = r.process (src[(size_t) i]); if (i >= 48000) y.push_back (v); }
        return y;
    };
    auto db = [&] (const std::vector<float>& y, double f0, double f1)
    {
        double acc = 0.0; int cnt = 0; const size_t n = y.size();
        for (double f = f0; f <= f1; f += (f1 - f0) / 24.0)
        {
            const double w = 2.0 * 3.14159265358979 * f / (double) fs; double re = 0, im = 0;
            for (size_t i = 0; i < n; i += 2)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) (n - 1));
                re += y[i] * win * std::cos (w * (double) i); im -= y[i] * win * std::sin (w * (double) i);
            }
            acc += 10.0 * std::log10 ((re * re + im * im) / (double) n + 1e-30); ++cnt;
        }
        return acc / cnt;
    };
    const auto soft = render (300.0f, 0.0f), half = render (300.0f, 0.5f), hp = render (300.0f, 1.0f);
    const double lowHalf = db (half, 40, 120) - db (soft, 40, 120), lowHp = db (hp, 40, 120) - db (soft, 40, 120);
    const double midHp = db (hp, 2000, 4000) - db (soft, 2000, 4000);
    std::printf ("pultec 300 Hz : graves (40-120 Hz) %+.1f dB a 50 %% ; %+.1f dB a 100 %% ; medium (2-4 kHz) %+.1f dB a 100 %%\n", lowHalf, lowHp, midHp);
    check (lowHalf < -3.0 && lowHalf > lowHp, "pultec : le plateau n'est pas progressif");
    check (lowHp < -10.0, "pultec : le passe-haut ne coupe pas assez les graves");
    check (std::fabs (midHp) < 1.5, "pultec : les mediums sont alteres");

    // à 0, la fréquence n'a aucun effet : exactement transparent
    const auto a = render (20.0f, 0.0f), b = render (600.0f, 0.0f);
    float dmax = 0.0f; for (size_t i = 0; i < a.size(); ++i) dmax = std::max (dmax, std::fabs (a[i] - b[i]));
    check (dmax == 0.0f, "pultec : pas transparent a 0 %");

    // stabilité aux réglages extrêmes
    Resonator r; r.prepare (fs);
    ResonatorParams p; p.feedback = 1.0f; p.lowCutHz = 600.0f; p.lowCutAmt = 1.0f; p.semitones = 30.0f; p.drive = 1.0f; p.bbd = 1.0f; p.noise = 1.0f;
    r.update (p); r.reset();
    float worst = 0.0f; bool finite = true;
    for (int i = 0; i < 48000 * 3; ++i) { const float v = r.process (i < 48000 ? rnd() : 0.0f); if (! std::isfinite (v)) finite = false; worst = std::max (worst, std::fabs (v)); }
    check (finite && worst < 8.0f, "pultec : instable aux reglages extremes");
}

// Séquenceur de pitch : position -> pas, liaison des pas désactivés, glissando, durées de pas
static void testSequencer()
{
    float v[16] = { 0, 7, 3, 12, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    bool on[16];
    for (bool& b : on) b = true;

    auto s = bbdres::sequencerState (5.3, 4, v, on, 0.0f);          // longueur 4 : le pas 5 retombe sur le pas 1
    check (s.index == 1 && s.cur == 7.0f && s.prev == 0.0f, "sequenceur : index apres bouclage");
    s = bbdres::sequencerState (-0.5, 4, v, on, 0.0f);              // position négative : dernier pas
    check (s.index == 3 && s.cur == 12.0f, "sequenceur : position negative");
    s = bbdres::sequencerState (1.0e9 + 0.25, 16, v, on, 0.0f);     // très loin dans le temps
    check (s.index >= 0 && s.index < 16, "sequenceur : grande position");

    on[2] = false;                                                  // pas 2 désactivé : il garde la hauteur du pas 1
    s = bbdres::sequencerState (2.4, 4, v, on, 0.0f);
    check (s.index == 2 && s.cur == 7.0f, "sequenceur : un pas desactive garde la hauteur precedente");
    for (bool& b : on) b = false;                                   // tout désactivé : décalage nul
    s = bbdres::sequencerState (3.2, 8, v, on, 0.0f);
    check (s.cur == 0.0f && s.prev == 0.0f, "sequenceur : tout desactive");
    for (bool& b : on) b = true;

    s = bbdres::sequencerState (1.25, 4, v, on, 0.5f);              // glide 50 % : à 25 % du pas, on est à mi-chemin
    check (std::fabs (s.t - 0.5f) < 1.0e-5f && s.prev == 0.0f && s.cur == 7.0f, "sequenceur : glissando a mi-chemin");
    s = bbdres::sequencerState (1.8, 4, v, on, 0.5f);
    check (s.t == 1.0f, "sequenceur : glissando termine");
    s = bbdres::sequencerState (1.0, 1, v, on, 1.0f);               // longueur 1 : aucun mouvement
    check (s.prev == s.cur, "sequenceur : longueur 1");

    check (std::fabs (bbdres::seqStepBeats (5, 4.0) - 1.0) < 1.0e-9, "duree : 1/4 = 1 temps");
    check (std::fabs (bbdres::seqStepBeats (2, 4.0) - 0.5) < 1.0e-9, "duree : 1/8 = 1/2 temps");
    check (std::fabs (bbdres::seqStepBeats (9, 4.0) - 2.0) < 1.0e-9, "duree : 1/2 mesure en 4/4");
    check (std::fabs (bbdres::seqStepBeats (9, 3.0) - 1.5) < 1.0e-9, "duree : 1/2 mesure en 3/4");
    check (std::fabs (bbdres::seqStepBeats (12, 4.0) - 16.0) < 1.0e-9, "duree : 4 mesures en 4/4");
    check (std::fabs (bbdres::seqStepBeats (12, 3.0) - 12.0) < 1.0e-9, "duree : 4 mesures en 3/4");
    check (bbdres::kNumSeqSteps == 13, "nombre de durees de pas");
    std::printf ("sequenceur : %d durees de pas, de %s a %s\n", bbdres::kNumSeqSteps, bbdres::kSeqSteps[0].name, bbdres::kSeqSteps[bbdres::kNumSeqSteps - 1].name);
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
    testLooper();
    testRelease();
    testHoldFilter();
    testResponseModel();
    testPultec();
    testSequencer();
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
