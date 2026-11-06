#pragma once
 
// Identifiants des paramètres : NE PLUS LES CHANGER une fois des presets
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
inline constexpr const char* bbd        = "bbd";         // caractère BBD 0..1
// Pitch
inline constexpr const char* root       = "root";        // tonique 0..11
inline constexpr const char* scale      = "scale";       // index dans kScales
inline constexpr const char* quantize   = "quantize";    // Tune calé sur la gamme
// Mod
inline constexpr const char* lfoShape   = "lfoShape";    // 0..5
inline constexpr const char* lfoRate    = "lfoRate";     // Hz
inline constexpr const char* lfoDepth   = "lfoDepth";    // 0..1
inline constexpr const char* lfoTarget  = "lfoTarget";   // 0 Tune, 1 Feedback, 2 Filter
inline constexpr const char* dynamics   = "dynamics";    // -1..+1 (suiveur d'enveloppe -> filtre)
inline constexpr const char* drift      = "drift";       // 0..1
// Output
inline constexpr const char* mix        = "mix";         // dry/wet 0..1
inline constexpr const char* outLevel   = "outLevel";    // niveau de sortie, dB
}
