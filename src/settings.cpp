#include "settings.h"
#include "chess_types.h"   // TimeControl enum
#include <Preferences.h>
#include <cstring>

// =====================================================================
// Settings persistence + resolution helpers.
// =====================================================================

static constexpr uint8_t SETTINGS_VERSION = 1;

static constexpr const char* NVS_NAMESPACE = "chess";
static constexpr const char* NVS_KEY       = "settings";

namespace Settings {

ChessSettings g;

// ── Defaults ────────────────────────────────────────────────────────

static void defaults(ChessSettings& s) {
    s.version          = SETTINGS_VERSION;
    s.brightness       = 4;   // index -> 128
    s.theme            = 0;   // Dark
    s.pieceStyle       = 0;   // sprites
    s.boardStyle       = 0;   // themed
    s.animation        = 1;   // normal
    s.showCoords       = 1;
    s.showLegalDots    = 1;
    s.showLastMove     = 1;
    s.cursorWrap       = 0;
    s.keyRepeat        = 1;   // normal
    s.autoFlip         = 1;
    s.autoSave         = 1;
    s.aiBook           = 1;
    s.defaultVariant   = 0;   // Standard
    s.defaultTimeCtrl  = static_cast<uint8_t>(TimeControl::None);
    s.espnowChannel    = 1;
    s.pairingTimeout   = 60;
    s.disconnectMs     = 3000;
    s.crc              = 0;
}

static uint8_t computeCrc(const ChessSettings& s) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&s);
    uint8_t crc = 0;
    // XOR every byte except the trailing crc field
    for (uint8_t i = 0; i < offsetof(ChessSettings, crc); i++) {
        crc ^= p[i];
    }
    return crc;
}

// ── Load / Save ─────────────────────────────────────────────────────

void load() {
    defaults(g);

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    size_t len = prefs.getBytesLength(NVS_KEY);

    if (len == sizeof(ChessSettings)) {
        ChessSettings tmp;
        prefs.getBytes(NVS_KEY, &tmp, sizeof(tmp));
        prefs.end();

        if (tmp.version == SETTINGS_VERSION) {
            uint8_t expected = computeCrc(tmp);
            if (tmp.crc == expected) {
                g = tmp;            // Valid — adopt
                return;
            }
        }
        // Corrupt or wrong version — reset and persist
        defaults(g);
        g.crc = computeCrc(g);
        save();
        return;
    }

    prefs.end();
    // No save yet — keep defaults, persist once
    g.crc = computeCrc(g);
    save();
}

void save() {
    g.version = SETTINGS_VERSION;
    g.crc = computeCrc(g);
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes(NVS_KEY, &g, sizeof(g));
    prefs.end();
}

void reset() {
    defaults(g);
    g.crc = computeCrc(g);
    save();
}

// ── Resolution ──────────────────────────────────────────────────────

static const uint8_t BRIGHTNESS_LEVELS[] = {0, 16, 32, 64, 96, 128, 160, 192, 224, 255};
static constexpr uint8_t BRIGHTNESS_COUNT = sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);

uint8_t brightnessValue() {
    uint8_t idx = g.brightness;
    if (idx >= BRIGHTNESS_COUNT) idx = BRIGHTNESS_COUNT / 2;  // mid
    return BRIGHTNESS_LEVELS[idx];
}

uint32_t keyRepeatDelayMs() {
    switch (g.keyRepeat) {
        case 0:  return 600;  // slow
        case 2:  return 250;  // fast
        default: return 400;  // normal
    }
}

uint32_t keyRepeatRateMs() {
    switch (g.keyRepeat) {
        case 0:  return 150;  // slow
        case 2:  return 50;   // fast
        default: return 80;   // normal
    }
}

uint32_t animDurationMs() {
    switch (g.animation) {
        case 0:  return 0;    // off
        case 2:  return 120;  // fast
        default: return 250;  // normal
    }
}

uint32_t pairingTimeoutMs() {
    switch (g.pairingTimeout) {
        case 30:  return 30000;
        case 120: return 120000;
        default:  return 60000;
    }
}

uint16_t disconnectMsValue() {
    switch (g.disconnectMs) {
        case 2000: return 2000;
        case 5000: return 5000;
        default:   return 3000;
    }
}

} // namespace Settings
