#pragma once

// Identifiants des paramètres de BBDRes V2 : NE PLUS LES CHANGER une fois des presets
// ou de l'automation enregistrés dans Live.
namespace ParamIDs
{
// Input
inline constexpr const char* drive      = "drive";       // 0..1
inline constexpr const char* driveCurve = "driveCurve";  // 0 Soft, 1 Asym, 2 Fold
inline constexpr const char* inputLP    = "inputLP";     // filtre d'entrée 12 dB, Hz
// Resonator
inline constexpr const char* tune       = "tune";        // note MIDI (C3 = 60)
inline constexpr const char* fine       = "fine";        // accord fin, en cents
inline constexpr const char* feedback   = "feedback";    // 0..1
inline constexpr const char* loopLP     = "loopLP";      // filtre de boucle 6 dB, Hz
inline constexpr const char* lowCut     = "lowCut";      // atténuateur de graves de la boucle : fréquence (20..600 Hz)
inline constexpr const char* pultec     = "pultec";      // 0 = SOFT (transparent) ... 1 = HP 6 dB
inline constexpr const char* loopDyn    = "loopDyn";     // -1..+1 : l'entrée ouvre/ferme le filtre de boucle
inline constexpr const char* bbd        = "bbd";         // caractère BBD 0..1 (bande passante + grain)
inline constexpr const char* noise      = "noise";       // souffle 0..1
inline constexpr const char* freeze     = "freeze";      // fige le son (hold)
inline constexpr const char* holdLen    = "holdLen";     // longueur de la boucle du looper, de 30 ms à 4 s
inline constexpr const char* holdQuant  = "holdQuant";   // longueur calée sur le tempo (divisions rythmiques)
inline constexpr const char* holdMix    = "holdMix";     // boucle contre son d'origine, 0..1
inline constexpr const char* holdHP     = "holdHP";      // passe-haut 12 dB du looper uniquement (20 Hz = ouvert)
inline constexpr const char* holdLP     = "holdLP";      // passe-bas 12 dB du looper uniquement (20 kHz = ouvert)
// Pitch
inline constexpr const char* root       = "root";        // tonique 0..11
inline constexpr const char* scale      = "scale";       // index dans kScales
inline constexpr const char* quantize   = "quantize";    // Tune calé sur la gamme
// Mod
inline constexpr const char* lfoShape   = "lfoShape";    // 0..5
inline constexpr const char* lfoSync    = "lfoSync";     // 0 = libre, sinon division du tempo
inline constexpr const char* lfoRate    = "lfoRate";     // Hz (mode libre)
inline constexpr const char* lfoPhase   = "lfoPhase";    // décalage de phase, 0..1 cycle (mode sync)
inline constexpr const char* lfoDepth   = "lfoDepth";    // 0..1
inline constexpr const char* lfoTarget  = "lfoTarget";   // 0 Tune, 1 Feedback, 2 Filter
inline constexpr const char* drift      = "drift";       // 0..1
// Stereo
inline constexpr const char* spread     = "spread";      // désaccord gauche/droite 0..1
inline constexpr const char* cross      = "cross";       // couplage gauche/droite 0..1
// Output
inline constexpr const char* mix        = "mix";         // dry/wet 0..1
inline constexpr const char* outLevel   = "outLevel";    // niveau de sortie, dB
}
