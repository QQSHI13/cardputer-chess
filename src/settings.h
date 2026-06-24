#ifndef SETTINGS_H
#define SETTINGS_H

#include <cstdint>

// =====================================================================
// User settings, persisted in NVS under namespace "chess" / key "settings".
//
// A single global `Settings::g` is loaded once at boot. Scenes read from
// it directly; the SettingsScene mutates it and calls save(). Multis are
// kept small (uint8_t) and a CRC byte guards against corruption — on any
// mismatch we fall back to safe defaults rather than boot into a bad state.
// =====================================================================

struct ChessSettings {
    uint8_t version;          // format version (bump when layout changes)
    uint8_t brightness;       // index into kBrightnessLevels
    uint8_t theme;            // 0=Dark 1=Light 2=HighContrast
    uint8_t pieceStyle;       // 0=sprites 1=letters
    uint8_t boardStyle;       // 0=themed 1=black&white
    uint8_t animation;        // 0=off 1=normal 2=fast
    uint8_t showCoords;       // 0/1  file/rank labels on board edge
    uint8_t showLegalDots;    // 0/1  dot marker on empty legal-move squares
    uint8_t showLastMove;     // 0/1  highlight from/to of last move
    uint8_t cursorWrap;       // 0/1  Grid cursor wraps at edges
    uint8_t keyRepeat;        // 0=slow 1=normal 2=fast
    uint8_t autoFlip;         // 0/1  flip board between turns in pass-and-play
    uint8_t autoSave;         // 0/1  persist game to NVS after each move
    uint8_t aiBook;           // 0/1  AI uses opening book (standard variant)
    uint8_t defaultVariant;   // 0=Standard 1=Chess960 (lobby default)
    uint8_t defaultTimeCtrl;  // TimeControl enum (lobby default)
    uint8_t espnowChannel;    // 1..11
    uint8_t pairingTimeout;   // seconds: 30/60/120
    uint16_t disconnectMs;    // online disconnect threshold: 2000/3000/5000
    uint8_t crc;              // XOR of all bytes above
};

namespace Settings {

    // The single in-memory settings instance. Loaded in setup().
    extern ChessSettings g;

    // Load from NVS; on any error/corruption reset to defaults (and persist).
    void load();

    // Persist current settings to NVS.
    void save();

    // Reset to factory defaults (and persist).
    void reset();

    // ── Resolved values (translate indices/enum-ish fields to real units) ──

    uint8_t  brightnessValue();    // 0..255
    uint32_t keyRepeatDelayMs();
    uint32_t keyRepeatRateMs();
    uint32_t animDurationMs();     // 0 = animations off
    uint32_t pairingTimeoutMs();
    uint16_t disconnectMsValue();

    // Convenience booleans
    inline bool autoFlip()      { return g.autoFlip != 0; }
    inline bool autoSave()      { return g.autoSave != 0; }
    inline bool aiBook()        { return g.aiBook != 0; }
    inline bool cursorWrap()    { return g.cursorWrap != 0; }
    inline bool showCoords()    { return g.showCoords != 0; }
    inline bool showLegalDots() { return g.showLegalDots != 0; }
    inline bool showLastMove()  { return g.showLastMove != 0; }

} // namespace Settings

#endif // SETTINGS_H
