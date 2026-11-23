#pragma once

// Rendu hors temps réel d'UN cycle de la boucle Freeze, tel que le looper la joue : même fenêtre, même point de
// boucle (début S), même longueur, même raccord fondu, et les mêmes filtres de boucle (Loop LP et BBD). Les filtres du
// Hold (HP / LP) ne sont PAS inclus : ils restent actifs à la lecture d'une mémoire. Vitesse 1 : le Drift n'y entre pas.

#include "Resonator.h"

namespace bbdres
{
inline float frozenAtStatic (const float* buf, int cap, float x)
{
    const int i0 = (int) x, hi = cap - 1;
    const float f = x - (float) i0;
    const float ym1 = buf[std::clamp (i0 - 1, 0, hi)], y0 = buf[std::clamp (i0, 0, hi)];
    const float y1 = buf[std::clamp (i0 + 1, 0, hi)], y2 = buf[std::clamp (i0 + 2, 0, hi)];
    const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

// Même lecture que Resonator::readHead : fin de boucle fondue vers ce qui précède le début
inline float readLoopAt (const LoopExport& e, float p)
{
    float s = frozenAtStatic (e.buf, e.cap, p);
    const float zone = (float) (e.E - e.X);
    if (p >= zone)
    {
        const float w = (p - zone) / (float) e.X;
        s = s * (1.0f - w) + frozenAtStatic (e.buf, e.cap, p - (float) e.L) * w;
    }
    return s;
}

// Ajoute (gain x un cycle de la boucle) à acc (taille e.L). Les filtres tournent trois fois sur le cycle : régime établi.
inline void renderLoopCycle (const LoopExport& e, float gain, std::vector<float>& acc)
{
    const int n = e.L;
    std::vector<float> cyc ((size_t) n);
    for (int q = 0; q < n; ++q)
        cyc[(size_t) q] = readLoopAt (e, (float) (e.S + q));
    OnePole lp;
    lp.a = e.loopA;
    Biquad bw;
    bw.b0 = e.b0; bw.b1 = e.b1; bw.b2 = e.b2; bw.a1 = e.a1; bw.a2 = e.a2;
    for (int pass = 0; pass < 3; ++pass)
        for (int q = 0; q < n; ++q)
        {
            float v = lp.process (cyc[(size_t) q]);
            v += (bw.process (v) - v) * e.bwMix;
            if (pass == 2) acc[(size_t) q] += gain * v;
        }
}

// Met un cycle au format d'une mémoire : pré-roll (copie de la fin du cycle) puis le cycle.
// Comme le raccord de fin est déjà dans le cycle, le fondu du lecteur vers le pré-roll est sans effet : la boucle
// rejouée est strictement celle du Freeze.
inline std::vector<float> makeSlotBuffer (const std::vector<float>& cycle, int preroll)
{
    const int n = (int) cycle.size();
    std::vector<float> b ((size_t) (preroll + n));
    for (int i = 0; i < preroll; ++i) b[(size_t) i] = cycle[(size_t) (n - preroll + i)];
    for (int i = 0; i < n; ++i) b[(size_t) (preroll + i)] = cycle[(size_t) i];
    return b;
}
} // namespace bbdres
