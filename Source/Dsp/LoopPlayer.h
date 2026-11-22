#pragma once

// Lecteur d'une boucle Freeze mémorisée (fil audio). Il rejoue en continu le cycle mémorisé, avec :
//  - le raccord fondu de la fin vers le pré-roll (comme le looper) ;
//  - la phase : décalage de la lecture, fondu de 12 ms, au plus un déplacement toutes les 20 ms (aucun clic) ;
//  - les filtres HP / LP du Hold (toujours actifs, transparents quand ils sont ouverts) ;
//  - l'enveloppe : entrée rapide (30 ms), sortie en rampe linéaire de 0,5 s, comme le Freeze.

#include "Resonator.h"

namespace bbdres
{
class LoopPlayer
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        fade = std::max (8, (int) (Resonator::kFadeSeconds * fs));
        minInterval = (int) (Resonator::kLenChangeSeconds * fs);
        slew = 1.0f - std::exp (-1.0f / (0.03f * fs));
        stopNow();
    }

    // Démarre la lecture. Les données (propriété de la banque) doivent rester valides tant que le lecteur est actif.
    bool start (const float* left, const float* right, int total, int prerollSamples, int slotIndex, float phaseNow)
    {
        if (total - prerollSamples < 16 || prerollSamples < fade) return false;
        L = left; R = right; pre = prerollSamples; len = total - prerollSamples; end = (float) total; count = total;
        phase = phaseApplied = std::clamp (phaseNow, 0.0f, 1.0f);
        head = (float) pre + phase * (float) len;
        wrap (head);
        xfLeft = 0; sinceMove = 0;
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
        slotId = slotIndex;
        playing = true;
        return true;
    }
    void stop()   // sortie en rampe de 0,5 s
    {
        if (! playing) return;
        playing = false;
        relStep = std::max (env, 1.0e-3f) / (Resonator::kReleaseSeconds * fs);
    }
    void stopNow() { playing = false; env = 0.0f; relStep = 0.0f; slotId = -1; xfLeft = 0; }

    bool isPlaying() const { return playing; }
    bool active() const { return playing || env > 0.0f; }
    int slot() const { return slotId; }
    float envelope() const { return env; }

    void setPhase (float p) { phase = std::clamp (p, 0.0f, 1.0f); }
    void setFilters (float hpHz, float lpHz)
    {
        const float hp = std::clamp (hpHz, 20.0f, 20000.0f), lp = std::clamp (lpHz, 20.0f, 20000.0f);
        hpAmt = std::clamp (std::log2 (hp / 20.0f), 0.0f, 1.0f);
        lpAmt = std::clamp (std::log2 (20000.0f / lp), 0.0f, 1.0f);
        hpL.setHighpass (hp, fs);
        lpL.setLowpass (lp, fs);
        copyCoefficients (hpR, hpL);   // mêmes coefficients ; l'ÉTAT de chaque canal reste le sien
        copyCoefficients (lpR, lpL);
    }

    void process (float& outL, float& outR)
    {
        if (! active()) { outL = outR = 0.0f; return; }
        if (xfLeft == 0 && sinceMove >= minInterval)   // phase : déplacement de la lecture avec fondu
        {
            float d = phase - phaseApplied;
            d -= std::round (d);
            if (std::fabs (d) * (float) len >= 24.0f)
            {
                prev = head;
                xfLeft = fade;
                head += d * (float) len;
                wrap (head);
                phaseApplied += d;
                sinceMove = 0;
            }
        }
        ++sinceMove;

        float l = read (L, head), r = read (R, head);
        head = advance (head);
        if (xfLeft > 0)
        {
            const float w = 1.0f - (float) xfLeft / (float) fade;
            l = read (L, prev) * (1.0f - w) + l * w;
            r = read (R, prev) * (1.0f - w) + r * w;
            prev = advance (prev);
            --xfLeft;
        }
        l += (hpL.process (l) - l) * hpAmt;  l += (lpL.process (l) - l) * lpAmt;
        r += (hpR.process (r) - r) * hpAmt;  r += (lpR.process (r) - r) * lpAmt;

        if (playing) env += (1.0f - env) * slew;
        else { env -= relStep; if (env < 1.0e-4f) { env = 0.0f; slotId = -1; } }
        outL = l;
        outR = r;
    }

private:
    static void copyCoefficients (Biquad& dst, const Biquad& src)
    {
        dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2; dst.a1 = src.a1; dst.a2 = src.a2;
    }

    float at (const float* b, float x) const
    {
        const int i0 = (int) x, hi = count - 1;
        const float f = x - (float) i0;
        const float ym1 = b[std::clamp (i0 - 1, 0, hi)], y0 = b[std::clamp (i0, 0, hi)];
        const float y1 = b[std::clamp (i0 + 1, 0, hi)], y2 = b[std::clamp (i0 + 2, 0, hi)];
        const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }
    float read (const float* b, float x) const
    {
        float s = at (b, x);
        const float zone = end - (float) fade;
        if (x >= zone)   // fin de boucle : on fond vers ce qui précède le début
        {
            const float w = (x - zone) / (float) fade;
            s = s * (1.0f - w) + at (b, x - (float) len) * w;
        }
        return s;
    }
    float advance (float x) const { x += 1.0f; if (x >= end) x -= (float) len; return x; }
    void wrap (float& x) const
    {
        while (x >= end) x -= (float) len;
        while (x < (float) pre) x += (float) len;
    }

    float fs = 48000.0f, slew = 0.001f, relStep = 0.0f, env = 0.0f, hpAmt = 0.0f, lpAmt = 0.0f;
    float head = 0.0f, prev = 0.0f, end = 0.0f, phase = 0.0f, phaseApplied = 0.0f;
    int fade = 8, minInterval = 960, pre = 0, len = 1, count = 0, xfLeft = 0, sinceMove = 0, slotId = -1;
    bool playing = false;
    const float* L = nullptr;
    const float* R = nullptr;
    Biquad hpL, hpR, lpL, lpR;
};
} // namespace bbdres
