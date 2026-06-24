#include "settings_scene.h"
#include "chess_storage.h"
#include "puzzle_storage.h"
#include "esp_now_transport.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

// =====================================================================
// SettingsScene implementation.
// =====================================================================

// ── Option tables ────────────────────────────────────────────────────

struct Opt { const char* label; uint16_t value; };

static const Opt OPT_BRIGHTNESS[] = {
    {"Off", 0}, {"16", 1}, {"32", 2}, {"64", 3}, {"96", 4},
    {"128", 5}, {"160", 6}, {"192", 7}, {"224", 8}, {"255", 9}
};

static const Opt OPT_THEME[] = {
    {"Dark", 0}, {"Light", 1}, {"High Contrast", 2}
};
static const Opt OPT_PIECES[]   = {{"Sprites", 0}, {"Letters", 1}};
static const Opt OPT_BOARD[]    = {{"Themed", 0}, {"Black & White", 1}};
static const Opt OPT_ANIM[]     = {{"Off", 0}, {"Normal", 1}, {"Fast", 2}};
static const Opt OPT_ONOFF[]    = {{"On", 1}, {"Off", 0}};
static const Opt OPT_OFFON[]    = {{"Off", 0}, {"On", 1}};
static const Opt OPT_KEYREPEAT[]= {{"Slow", 0}, {"Normal", 1}, {"Fast", 2}};
static const Opt OPT_VARIANT[]  = {{"Standard", 0}, {"Chess960", 1}};
static const Opt OPT_TIME[] = {
    {"No Timer", 0}, {"1+0", 1}, {"3+2", 2}, {"5+3", 3}, {"10+0", 4}
};
static const Opt OPT_PAIR[]     = {{"30s", 30}, {"60s", 60}, {"120s", 120}};
static const Opt OPT_DISC[]     = {{"2s", 2000}, {"3s", 3000}, {"5s", 5000}};

// ESP-NOW channels 1..11 (built dynamically below)
static const Opt* s_channelOpts = nullptr;
static Opt s_channelBuf[11];
static void ensureChannelOpts() {
    if (s_channelOpts) return;
    for (uint8_t i = 0; i < 11; i++) {
        static char chanLbl[11][4];
        snprintf(chanLbl[i], sizeof(chanLbl[i]), "%d", i + 1);
        s_channelBuf[i] = {chanLbl[i], (uint8_t)(i + 1)};
    }
    s_channelOpts = s_channelBuf;
}

// ── Row metadata ─────────────────────────────────────────────────────

static constexpr uint8_t ROW_BRIGHTNESS   = 0;
static constexpr uint8_t ROW_THEME        = 1;
static constexpr uint8_t ROW_PIECES       = 2;
static constexpr uint8_t ROW_BOARD        = 3;
static constexpr uint8_t ROW_ANIM         = 4;
static constexpr uint8_t ROW_COORDS       = 5;
static constexpr uint8_t ROW_LEGALDOTS    = 6;
static constexpr uint8_t ROW_LASTMOVE     = 7;
static constexpr uint8_t ROW_CURSORWRAP   = 8;
static constexpr uint8_t ROW_KEYREPEAT    = 9;
static constexpr uint8_t ROW_AUTOFILP     = 10;
static constexpr uint8_t ROW_AUTOSAVE     = 11;
static constexpr uint8_t ROW_AIBOOK       = 12;
static constexpr uint8_t ROW_DEFVARIANT   = 13;
static constexpr uint8_t ROW_DEFTIME      = 14;
static constexpr uint8_t ROW_CHANNEL      = 15;
static constexpr uint8_t ROW_PAIRTIMEOUT  = 16;
static constexpr uint8_t ROW_DISCONNECT   = 17;
static constexpr uint8_t ROW_CLEARSAVE    = 18;
static constexpr uint8_t ROW_RESETPUZZ    = 19;
static constexpr uint8_t ROW_FACTORY      = 20;
static constexpr uint8_t ROW_ABOUT        = 21;
static constexpr uint8_t ROW_BACK         = 22;
static constexpr uint8_t ROW_COUNT        = 23;

static const char* rowName(uint8_t idx) {
    switch (idx) {
    case ROW_BRIGHTNESS:  return "Brightness";
    case ROW_THEME:       return "Theme";
    case ROW_PIECES:      return "Pieces";
    case ROW_BOARD:       return "Board";
    case ROW_ANIM:        return "Animation";
    case ROW_COORDS:      return "Coordinates";
    case ROW_LEGALDOTS:   return "Legal dots";
    case ROW_LASTMOVE:    return "Last-move mark";
    case ROW_CURSORWRAP:  return "Cursor wrap";
    case ROW_KEYREPEAT:   return "Key repeat";
    case ROW_AUTOFILP:    return "Auto-flip";
    case ROW_AUTOSAVE:    return "Auto-save";
    case ROW_AIBOOK:      return "AI opening book";
    case ROW_DEFVARIANT:  return "Default variant";
    case ROW_DEFTIME:     return "Default time";
    case ROW_CHANNEL:     return "ESP-NOW channel";
    case ROW_PAIRTIMEOUT: return "Pairing timeout";
    case ROW_DISCONNECT:  return "Disconnect timeout";
    case ROW_CLEARSAVE:   return "Clear saved game";
    case ROW_RESETPUZZ:   return "Reset puzzles";
    case ROW_FACTORY:     return "Factory reset";
    case ROW_ABOUT:       return "About";
    case ROW_BACK:        return "Back";
    default:              return "";
    }
}

// Fill `opts` with the option list for a setting row. Returns count.
// `current` is set to the index in `opts` matching the live value.
static uint8_t fillOptions(uint8_t rowIdx, Opt* opts, uint8_t& current) {
    const Opt* src = nullptr;
    uint8_t count = 0;
    uint16_t curVal = 0;
    current = 0;

    switch (rowIdx) {
    case ROW_BRIGHTNESS:  src = OPT_BRIGHTNESS;   count = 10; curVal = Settings::g.brightness; break;
    case ROW_THEME:       src = OPT_THEME;        count = 3;  curVal = Settings::g.theme; break;
    case ROW_PIECES:      src = OPT_PIECES;       count = 2;  curVal = Settings::g.pieceStyle; break;
    case ROW_BOARD:       src = OPT_BOARD;        count = 2;  curVal = Settings::g.boardStyle; break;
    case ROW_ANIM:        src = OPT_ANIM;         count = 3;  curVal = Settings::g.animation; break;
    case ROW_COORDS:      src = OPT_ONOFF;        count = 2;  curVal = Settings::g.showCoords; break;
    case ROW_LEGALDOTS:   src = OPT_ONOFF;        count = 2;  curVal = Settings::g.showLegalDots; break;
    case ROW_LASTMOVE:    src = OPT_ONOFF;        count = 2;  curVal = Settings::g.showLastMove; break;
    case ROW_CURSORWRAP:  src = OPT_OFFON;        count = 2;  curVal = Settings::g.cursorWrap; break;
    case ROW_KEYREPEAT:   src = OPT_KEYREPEAT;    count = 3;  curVal = Settings::g.keyRepeat; break;
    case ROW_AUTOFILP:    src = OPT_ONOFF;        count = 2;  curVal = Settings::g.autoFlip; break;
    case ROW_AUTOSAVE:    src = OPT_ONOFF;        count = 2;  curVal = Settings::g.autoSave; break;
    case ROW_AIBOOK:      src = OPT_ONOFF;        count = 2;  curVal = Settings::g.aiBook; break;
    case ROW_DEFVARIANT:  src = OPT_VARIANT;      count = 2;  curVal = Settings::g.defaultVariant; break;
    case ROW_DEFTIME:     src = OPT_TIME;         count = 5;  curVal = Settings::g.defaultTimeCtrl; break;
    case ROW_CHANNEL:
        ensureChannelOpts();
        src = s_channelOpts; count = 11; curVal = Settings::g.espnowChannel;
        break;
    case ROW_PAIRTIMEOUT: src = OPT_PAIR;         count = 3;  curVal = Settings::g.pairingTimeout; break;
    case ROW_DISCONNECT:  src = OPT_DISC;         count = 3;  curVal = Settings::g.disconnectMs; break;
    default: return 0;
    }

    for (uint8_t i = 0; i < count; i++) {
        opts[i] = src[i];
        if (src[i].value == curVal) current = i;
    }
    return count;
}

// =====================================================================
// SettingsScene
// =====================================================================

SettingsScene::SettingsScene() : Scene("settings") {}

void SettingsScene::setup() {
    m_statusBar.setBounds({0, 0, SCREEN_W, 12});
    m_statusBar.setDrawSeparator(true);
    m_statusBar.setLeft("Settings");
    m_statusBar.setRight("");
    addWidget(&m_statusBar);

    m_list.setBounds({0, 14, SCREEN_W, SCREEN_H - 14});
    m_list.setItemHeight(11);
    m_list.setAutoScroll(true);
    m_list.setOnSelect([this](uint8_t index, const char*) {
        if (m_mode == Mode::List) {
            openPicker(index);
        } else if (m_mode == Mode::Picker) {
            // index selects an option
            if (index < m_optCount) {
                applyOption(m_focusRow, m_optValues[index]);
            }
            m_mode = Mode::List;
            rebuildList();
            focusChain().focusWidget(&m_list);
        }
    });
    addWidget(&m_list, true);

    m_modal.setBounds({0, 0, SCREEN_W, SCREEN_H});
    m_modal.setVisible(false);
    addWidget(&m_modal, true);
}

void SettingsScene::onEnter() {
    m_mode = Mode::List;
    m_statusBar.setLeft("Settings");
    m_statusBar.setRight("");
    rebuildList();
    m_modal.hide();
    focusChain().focusWidget(&m_list);
}

void SettingsScene::onTick(uint32_t) {
    // Static screen — nothing to update.
}

bool SettingsScene::onInput(const InputEvent& event) {
    if (!event.isDown()) return false;

    if (event.key == Key::ESCAPE) {
        if (m_mode == Mode::Picker) {
            m_mode = Mode::List;
            m_statusBar.setLeft("Settings");
            m_statusBar.setRight("");
            rebuildList();
            focusChain().focusWidget(&m_list);
            return true;
        }
        if (m_modal.isVisible()) {
            m_modal.hide();
            focusChain().focusWidget(&m_list);
            return true;
        }
        CardGFX::scenes().pop();
        return true;
    }
    return false;
}

// ── List building ────────────────────────────────────────────────────

static void formatRow(uint8_t idx, char* buf, uint8_t bufLen) {
    const char* name = rowName(idx);
    char valBuf[20] = "";

    Opt opts[16];
    uint8_t cur = 0;
    uint8_t n = fillOptions(idx, opts, cur);

    if (n > 0 && cur < n) {
        snprintf(valBuf, sizeof(valBuf), ": %s", opts[cur].label);
    }
    snprintf(buf, bufLen, "%s%s", name, valBuf);
}

void SettingsScene::rebuildList() {
    m_list.clearItems();
    if (m_mode == Mode::List) {
        for (uint8_t i = 0; i < ROW_COUNT; i++) {
            char buf[40];
            formatRow(i, buf, sizeof(buf));
            m_list.addItem(buf);
        }
        m_list.setSelected(m_focusRow < ROW_COUNT ? m_focusRow : 0);
    } else {
        // Picker mode — show options for the focused setting.
        Opt opts[16];
        uint8_t cur = 0;
        m_optCount = fillOptions(m_focusRow, opts, cur);
        m_pickerCurrent = cur;
        for (uint8_t i = 0; i < m_optCount; i++) {
            m_optValues[i] = opts[i].value;
            // Prefix current with a marker.
            char buf[40];
            snprintf(buf, sizeof(buf), "%s%s",
                     (i == cur) ? "> " : "  ",
                     opts[i].label);
            m_list.addItem(buf);
        }
        m_list.setSelected(cur);
    }
}

// ── Live application ─────────────────────────────────────────────────

void SettingsScene::applyBrightnessLive() {
    CardGFX::HAL::setBrightness(Settings::brightnessValue());
}

void SettingsScene::applyTheme() {
    switch (Settings::g.theme) {
        case 1:  CardGFX::setTheme(Themes::Light); break;
        case 2:  CardGFX::setTheme(Themes::HighContrast); break;
        default: CardGFX::setTheme(Themes::Dark); break;
    }
}

void SettingsScene::openPicker(uint8_t rowIdx) {
    // Action / special rows don't use the option list.
    switch (rowIdx) {
    case ROW_CLEARSAVE:
        confirmAction("Clear saved game?", []() {
            ChessStorage::clearSave();
        });
        return;
    case ROW_RESETPUZZ:
        confirmAction("Reset puzzle progress?", []() {
            PuzzleProgress p;
            memset(&p, 0, sizeof(p));
            PuzzleStorage::saveProgress(p);
        });
        return;
    case ROW_FACTORY:
        confirmAction("Factory reset all settings?", [this]() {
            Settings::reset();
            ChessStorage::clearSave();
            PuzzleProgress p;
            memset(&p, 0, sizeof(p));
            PuzzleStorage::saveProgress(p);
            // Re-apply live display settings so the screen reflects defaults.
            applyTheme();
            applyBrightnessLive();
            CardGFX::input().setRepeatTimings(
                Settings::keyRepeatDelayMs(), Settings::keyRepeatRateMs());
        });
        return;
    case ROW_ABOUT:
        showAbout();
        return;
    case ROW_BACK:
        CardGFX::scenes().pop();
        return;
    default:
        break;
    }

    uint8_t cur = 0;
    Opt tmp[16];
    uint8_t n = fillOptions(rowIdx, tmp, cur);
    if (n == 0) return;
    // m_optValues filled by rebuildList(); just record focus + mode here.
    m_focusRow = rowIdx;
    m_mode = Mode::Picker;
    m_pickerCurrent = cur;
    rebuildList();
    m_statusBar.setLeft(rowName(rowIdx));
    m_statusBar.setRight("ESC=Back");
    focusChain().focusWidget(&m_list);
}

void SettingsScene::applyOption(uint8_t rowIdx, uint16_t value) {
    switch (rowIdx) {
    case ROW_BRIGHTNESS:  Settings::g.brightness = value; applyBrightnessLive(); break;
    case ROW_THEME:       Settings::g.theme = value; applyTheme(); break;
    case ROW_PIECES:      Settings::g.pieceStyle = value; break;
    case ROW_BOARD:       Settings::g.boardStyle = value; break;
    case ROW_ANIM:        Settings::g.animation = value; break;
    case ROW_COORDS:      Settings::g.showCoords = value; break;
    case ROW_LEGALDOTS:   Settings::g.showLegalDots = value; break;
    case ROW_LASTMOVE:    Settings::g.showLastMove = value; break;
    case ROW_CURSORWRAP:  Settings::g.cursorWrap = value; break;
    case ROW_KEYREPEAT:
        Settings::g.keyRepeat = value;
        CardGFX::input().setRepeatTimings(
            Settings::keyRepeatDelayMs(), Settings::keyRepeatRateMs());
        break;
    case ROW_AUTOFILP:    Settings::g.autoFlip = value; break;
    case ROW_AUTOSAVE:    Settings::g.autoSave = value; break;
    case ROW_AIBOOK:      Settings::g.aiBook = value; break;
    case ROW_DEFVARIANT:  Settings::g.defaultVariant = value; break;
    case ROW_DEFTIME:     Settings::g.defaultTimeCtrl = value; break;
    case ROW_CHANNEL:     Settings::g.espnowChannel = value; break;
    case ROW_PAIRTIMEOUT: Settings::g.pairingTimeout = value; break;
    case ROW_DISCONNECT:  Settings::g.disconnectMs = value; break;
    default: return;
    }
    Settings::save();

    // Reset status bar label to the list view.
    m_statusBar.setLeft("Settings");
    m_statusBar.setRight("");
}

// ── Confirmation & About (Modal-based) ───────────────────────────────

void SettingsScene::confirmAction(const char* title, std::function<void()> action) {
    m_modal.clearButtons();
    m_modal.setEscapeCallback([]() {});
    m_modal.setTitle(title);
    m_modal.setMessage("");
    m_modal.addButton("Yes", [this, action]() {
        action();
        m_modal.hide();
        rebuildList();
        focusChain().focusWidget(&m_list);
    });
    m_modal.addButton("No", [this]() {
        m_modal.hide();
        focusChain().focusWidget(&m_list);
    });
    m_modal.show();
    focusChain().focusWidget(&m_modal);
}

void SettingsScene::showAbout() {
    const uint8_t* mac = EspNowTransport::instance().ownMac();
    char msg[80];
    snprintf(msg, sizeof(msg),
             "MAC %02X:%02X:%02X:%02X:%02X:%02X  Heap:%u",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             (unsigned)ESP.getFreeHeap());

    char title[24];
    snprintf(title, sizeof(title), "About v%s", FIRMWARE_VERSION);

    m_modal.clearButtons();
    m_modal.setTitle(title);
    m_modal.setMessage(msg);
    m_modal.addButton("Back", [this]() {
        m_modal.hide();
        focusChain().focusWidget(&m_list);
    });
    m_modal.show();
    focusChain().focusWidget(&m_modal);
}
