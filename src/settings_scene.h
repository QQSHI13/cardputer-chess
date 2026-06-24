#ifndef SETTINGS_SCENE_H
#define SETTINGS_SCENE_H

#include <cardgfx.h>
#include "settings.h"
#include <functional>

using namespace CardGFX;

// =====================================================================
// SettingsScene: scrollable list of all user settings + data actions.
//
// Two List modes share one widget:
//   - List mode   : rows of "Setting: value" plus action/back rows.
//   - Picker mode : the option list for the focused setting.
// Selecting a row in List mode opens the picker (or runs an action);
// selecting an option in Picker mode applies it, persists, and returns.
// The Modal is used only for destructive-action confirmations and the
// About screen (few buttons, fits Modal's 8-button cap).
//
// Esc returns to the lobby. Brightness / theme / key-repeat take effect
// live; other settings apply on the next game start.
// =====================================================================

class SettingsScene : public Scene {
public:
    SettingsScene();
    void setup();

    void onEnter() override;
    void onTick(uint32_t dt_ms) override;
    bool onInput(const InputEvent& event) override;

    // Apply the persisted theme to the CardGFX framework. Public so main()
    // can call it at boot and so it can be re-applied after a factory reset.
    static void applyTheme();

private:
    enum class Mode : uint8_t { List, Picker };

    Mode     m_mode = Mode::List;
    uint8_t  m_focusRow = 0;       // Setting row the picker is editing
    uint8_t  m_pickerCurrent = 0;  // Index of current value in picker

    // Option value buffer reused by picker mode (labels live in the List items).
    static constexpr uint8_t MAX_OPTS = 16;
    uint16_t m_optValues[MAX_OPTS] = {};
    uint8_t  m_optCount = 0;

    StatusBar m_statusBar;
    List      m_list;
    Modal     m_modal;

    void rebuildList();
    void openPicker(uint8_t rowIdx);
    void applyOption(uint8_t rowIdx, uint16_t value);
    void applyBrightnessLive();
    void confirmAction(const char* title, std::function<void()> action);
    void showAbout();
};

#endif // SETTINGS_SCENE_H
