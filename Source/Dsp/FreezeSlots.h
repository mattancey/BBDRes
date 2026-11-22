#pragma once

// Mémoires de boucles Freeze : stockage, enregistrement et conversion (C++ standard uniquement, testés hors du plug-in).
// Une mémoire contient UN cycle de la boucle gelée, stéréo, tel qu'on l'entend AVANT les filtres HP / LP du Hold.
// Elle est précédée d'un court pré-roll (12 ms) : la fin de boucle fond vers lui, comme dans le looper, ce qui raccorde
// la boucle sans clic même si la boucle d'origine n'était pas parfaitement périodique.
// Ces mémoires sont enregistrées avec le PROJET (état du plug-in), jamais avec les presets.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace bbdres
{
constexpr int kNumFreezeSlots = 4;
constexpr float kSlotMaxSeconds = 4.1f;   // boucle de 4 s au plus, plus le pré-roll

// ---- conversion pour l'état du projet : int16 stéréo, canal gauche puis canal droit ----
inline std::vector<std::int16_t> encodeSlot (const float* l, const float* r, int n)
{
    std::vector<std::int16_t> v ((size_t) n * 2);
    for (int i = 0; i < n; ++i)
    {
        v[(size_t) i] = (std::int16_t) std::lround (std::clamp (l[i], -1.0f, 1.0f) * 32767.0f);
        v[(size_t) (n + i)] = (std::int16_t) std::lround (std::clamp (r[i], -1.0f, 1.0f) * 32767.0f);
    }
    return v;
}

inline bool decodeSlot (const std::int16_t* data, size_t count, std::vector<float>& l, std::vector<float>& r)
{
    if (count < 2 || count % 2 != 0) return false;
    const size_t n = count / 2;
    l.resize (n);
    r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        l[i] = (float) data[i] / 32767.0f;
        r[i] = (float) data[n + i] / 32767.0f;
    }
    return true;
}

// ---- changement de fréquence d'échantillonnage (interpolation cubique) ----
inline std::vector<float> resampleCubic (const std::vector<float>& in, float srcSr, float dstSr)
{
    if (in.size() < 4 || srcSr <= 0.0f || dstSr <= 0.0f || std::fabs (srcSr - dstSr) < 0.5f) return in;
    const size_t m = (size_t) std::max (4.0, std::floor ((double) in.size() * (double) dstSr / (double) srcSr));
    std::vector<float> out (m);
    const int last = (int) in.size() - 1;
    for (size_t i = 0; i < m; ++i)
    {
        const double pos = (double) i * (double) srcSr / (double) dstSr;
        const int i0 = (int) pos;
        const float x = (float) (pos - (double) i0);
        const float ym1 = in[(size_t) std::clamp (i0 - 1, 0, last)], y0 = in[(size_t) std::clamp (i0, 0, last)];
        const float y1 = in[(size_t) std::clamp (i0 + 1, 0, last)], y2 = in[(size_t) std::clamp (i0 + 2, 0, last)];
        const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        out[i] = ((c3 * x + c2) * x + c1) * x + y0;
    }
    return out;
}

// ---- stockage des quatre mémoires ----
// L'interface (fil principal) lit les formes d'onde et enregistre l'état ; le fil audio y écrit la fin d'un
// enregistrement. Un compteur de version (impair pendant une écriture) permet à l'interface de relire si besoin.
struct Slot
{
    std::unique_ptr<float[]> left, right;
    int capacity = 0;
    int preroll = 0;                       // échantillons de pré-roll au début
    std::atomic<int> total { 0 };          // échantillons mémorisés, pré-roll compris (0 = vide)
    std::atomic<bool> ready { false };
    std::atomic<unsigned> version { 0 };
};

class FreezeSlotBank
{
public:
    bool isPrepared() const { return fs > 0.0f; }
    float sampleRate() const { return fs; }

    // Fil principal, audio à l'arrêt : (ré)alloue pour la fréquence d'échantillonnage, en rééchantillonnant le contenu
    void prepare (float sampleRate)
    {
        const float oldFs = fs;
        std::array<Data, kNumFreezeSlots> keep;
        for (int i = 0; i < kNumFreezeSlots; ++i)
            keep[(size_t) i] = pending[(size_t) i].empty() ? read (i, oldFs) : std::move (pending[(size_t) i]);
        fs = sampleRate;
        const int cap = (int) (kSlotMaxSeconds * fs) + 64;
        for (int i = 0; i < kNumFreezeSlots; ++i)
        {
            auto& s = slots[(size_t) i];
            s.ready.store (false);
            s.total.store (0);
            s.left.reset (new float[(size_t) cap]());
            s.right.reset (new float[(size_t) cap]());
            s.capacity = cap;
            pending[(size_t) i] = {};
            if (! keep[(size_t) i].empty()) install (i, keep[(size_t) i]);
        }
    }

    // Fil principal : installe des données venues de l'état du projet (reportées jusqu'à prepare() si besoin)
    void load (int i, std::vector<float> l, std::vector<float> r, float srcSr, int preroll)
    {
        Data d { std::move (l), std::move (r), srcSr, preroll };
        if (! isPrepared()) pending[(size_t) i] = std::move (d);
        else install (i, d);
    }

    void clearAll()
    {
        for (int i = 0; i < kNumFreezeSlots; ++i)
        {
            pending[(size_t) i] = {};
            slots[(size_t) i].ready.store (false);
            slots[(size_t) i].total.store (0);
            slots[(size_t) i].version.fetch_add (2);
        }
    }

    // Fil audio : fin d'un enregistrement (copie sans allocation)
    void commit (int i, const float* l, const float* r, int n, int preroll)
    {
        auto& s = slots[(size_t) i];
        n = std::min (n, s.capacity);
        s.ready.store (false, std::memory_order_release);
        s.version.fetch_add (1, std::memory_order_acq_rel);
        std::memcpy (s.left.get(), l, (size_t) n * sizeof (float));
        std::memcpy (s.right.get(), r, (size_t) n * sizeof (float));
        s.preroll = preroll;
        s.total.store (n, std::memory_order_release);
        s.version.fetch_add (1, std::memory_order_acq_rel);
        s.ready.store (true, std::memory_order_release);
    }

    bool used (int i) const { return slots[(size_t) i].ready.load (std::memory_order_acquire) && slots[(size_t) i].total.load() > 0; }
    const float* leftData (int i) const { return slots[(size_t) i].left.get(); }
    const float* rightData (int i) const { return slots[(size_t) i].right.get(); }
    int total (int i) const { return slots[(size_t) i].total.load (std::memory_order_acquire); }
    int preroll (int i) const { return slots[(size_t) i].preroll; }
    unsigned version (int i) const { return slots[(size_t) i].version.load (std::memory_order_acquire); }

    // Fil principal : forme d'onde (crête par tranche) de la boucle, sans le pré-roll
    std::vector<float> peaks (int i, int buckets) const
    {
        std::vector<float> out ((size_t) std::max (1, buckets), 0.0f);
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const unsigned v0 = version (i);
            if ((v0 & 1u) != 0u || ! used (i)) continue;
            const auto& s = slots[(size_t) i];
            const int n = s.total.load(), pre = s.preroll, len = n - pre;
            if (len < buckets) break;
            for (int b = 0; b < buckets; ++b)
            {
                const int a = pre + (int) ((long long) len * b / buckets), e = pre + (int) ((long long) len * (b + 1) / buckets);
                float m = 0.0f;
                for (int k = a; k < e; ++k) m = std::max (m, std::max (std::fabs (s.left[k]), std::fabs (s.right[k])));
                out[(size_t) b] = m;
            }
            if (version (i) == v0) return out;
        }
        return out;
    }

    // Fil principal : contenu pour l'état du projet (faux si la mémoire est vide ou en cours d'écriture)
    bool exportSlot (int i, std::vector<float>& l, std::vector<float>& r, float& sr, int& pre) const
    {
        if (! isPrepared())   // état chargé mais audio pas encore préparé : on rend les données en attente
        {
            const auto& d = pending[(size_t) i];
            if (d.empty()) return false;
            l = d.l; r = d.r; sr = d.sr; pre = d.pre;
            return true;
        }
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const unsigned v0 = version (i);
            if ((v0 & 1u) != 0u || ! used (i)) return false;
            const auto& s = slots[(size_t) i];
            const int n = s.total.load();
            l.assign (s.left.get(), s.left.get() + n);
            r.assign (s.right.get(), s.right.get() + n);
            sr = fs;
            pre = s.preroll;
            if (version (i) == v0) return true;
        }
        return false;
    }

private:
    struct Data
    {
        std::vector<float> l, r;
        float sr = 0.0f;
        int pre = 0;
        bool empty() const { return l.empty(); }
    };

    Data read (int i, float srcFs) const
    {
        Data d;
        if (! used (i)) return d;
        float sr = 0.0f;
        int pre = 0;
        exportSlot (i, d.l, d.r, sr, pre);
        d.sr = srcFs;
        d.pre = pre;
        return d;
    }

    void install (int i, Data d)
    {
        auto& s = slots[(size_t) i];
        if (fs > 0.0f && d.sr > 0.0f && std::fabs (d.sr - fs) > 0.5f)
        {
            d.l = resampleCubic (d.l, d.sr, fs);
            d.r = resampleCubic (d.r, d.sr, fs);
            d.pre = (int) std::lround ((double) d.pre * (double) fs / (double) d.sr);
        }
        const int n = (int) std::min<size_t> (std::min (d.l.size(), d.r.size()), (size_t) s.capacity);
        if (n <= d.pre + 16) return;
        s.ready.store (false);
        s.version.fetch_add (1);
        std::copy (d.l.begin(), d.l.begin() + n, s.left.get());
        std::copy (d.r.begin(), d.r.begin() + n, s.right.get());
        s.preroll = d.pre;
        s.total.store (n);
        s.version.fetch_add (1);
        s.ready.store (true);
    }

    float fs = 0.0f;
    std::array<Slot, kNumFreezeSlots> slots;
    std::array<Data, kNumFreezeSlots> pending;
};

// ---- enregistreur : écoute la boucle gelée pendant (pré-roll + boucle) échantillons (fil audio) ----
class LoopRecorder
{
public:
    void prepare (float sampleRate)
    {
        const size_t cap = (size_t) (kSlotMaxSeconds * sampleRate) + 64;
        l.assign (cap, 0.0f);
        r.assign (cap, 0.0f);
        pos = total = 0;
        running = false;
    }
    bool start (int slotIndex, int loopLen, int prerollSamples)
    {
        if (loopLen < 16 || (size_t) (loopLen + prerollSamples) > l.size()) return false;
        slot = slotIndex; pre = prerollSamples; total = loopLen + prerollSamples; pos = 0; running = true;
        return true;
    }
    void abort() { running = false; }
    bool active() const { return running; }
    int slotIndex() const { return slot; }
    int preroll() const { return pre; }
    int length() const { return total; }
    const float* left() const { return l.data(); }
    const float* right() const { return r.data(); }
    // renvoie vrai quand l'enregistrement est terminé
    bool push (float a, float b)
    {
        l[(size_t) pos] = a;
        r[(size_t) pos] = b;
        if (++pos >= total) { running = false; return true; }
        return false;
    }

private:
    std::vector<float> l, r;
    int pos = 0, total = 0, slot = -1, pre = 0;
    bool running = false;
};
} // namespace bbdres
