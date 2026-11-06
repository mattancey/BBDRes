#pragma once
 
// Identifiants des paramètres : NE PLUS LES CHANGER une fois des presets
// ou de l'automation enregistrés dans Live.
namespace ParamIDs
{
inline constexpr const char* drive      = "drive";       // 0..1
inline constexpr const char* driveCurve = "driveCurve";  // 0 Soft, 1 Asym, 2 Fold
inline constexpr const char* inputLP    = "inputLP";     // filtre d'entrée 12 dB, Hz
inline constexpr const char* tune       = "tune";        // note MIDI (C3 = 60)
inline constexpr const char* fine       = "fine";        // accord fin, en cents
inline constexpr const char* feedback   = "feedback";    // 0..1
inline constexpr const char* loopLP     = "loopLP";      // filtre de boucle 6 dB, Hz
inline constexpr const char* bbd        = "bbd";         // caractère BBD 0..1
inline constexpr const char* mix        = "mix";         // dry/wet 0..1
inline constexpr const char* outLevel   = "outLevel";    // niveau de sortie, dB
}
 
