#pragma once
 
// Coeur DSP de BBDRes : C++ standard uniquement (aucune dépendance JUCE),
// pour pouvoir être testé séparément du plug-in.
 
#include <algorithm>
#include <cmath>
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
 
    // Retard de phase (en échantillons) à la fréquence f
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
 
// Résonateur : filtre en peigne à feedback positif, accordé par le temps de retard
class Resonator
{
public:
    static constexpr float kMaxLoopGain = 1.02f;  // > 1 : auto-oscillation possible
    static constexpr float kMinFreqHz = 20.0f;
 
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        delay.prepare ((int) (fs / kMinFreqHz) + 8);
        maxDelay = (float) delay.size() - 6.0f;
        dc.prepare (fs);
        update (48.0f, 0.5f, 8000.0f, 6000.0f);
        reset();
    }
    void reset()
    {
        delay.reset(); inLp.reset(); loop.reset(); dc.reset();
    }
 
    // semitones : note MIDI (peut être fractionnaire), feedback : 0..1
    void update (float semitones, float feedback, float inputLpHz, float loopLpHz)
    {
        const float fb = std::clamp (feedback, 0.0f, 1.0f);
        const float f0 = 440.0f * std::exp2 ((semitones - 69.0f) / 12.0f);
        loopGain = fb * kMaxLoopGain;
        inGain = 1.0f - 0.8f * fb;
        loop.setCutoff (loopLpHz, fs);
        inLp.setLowpass (inputLpHz, fs);
        // Le filtre de boucle retarde la phase : on le compense pour rester juste
        const float d = fs / f0 - loop.phaseDelay (f0, fs);
        delaySamples = std::clamp (d, 2.0f, maxDelay);
    }
 
    float process (float x)
    {
        const float in = dc.process (inLp.process (x)) * inGain;
        const float v = loop.process (delay.read (delaySamples));
        const float y = in + std::tanh (loopGain * v);  // saturation douce dans la boucle
        delay.push (y);
        return y;
    }
 
private:
    float fs = 48000.0f, maxDelay = 1000.0f, delaySamples = 100.0f;
    float loopGain = 0.0f, inGain = 1.0f;
    DelayLine delay;
    Biquad inLp;
    OnePole loop;
    DcBlocker dc;
};
} // namespace bbdres
 
