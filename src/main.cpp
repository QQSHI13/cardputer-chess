#include <Arduino.h>
#include <cardgfx.h>
#include "chess_scene.h"
#include "lobby_scene.h"
#include "settings_scene.h"
#include "chess_opening_book.h"
#include "settings.h"

using namespace CardGFX;

static ChessScene chessScene;
static LobbyScene lobbyScene;
static SettingsScene settingsScene;

// =====================================================================
// Setup & Loop
// =====================================================================

void setup() {
    // Load persisted user settings first so brightness/theme are correct
    // from the very first frame.
    Settings::load();

    if (!CardGFX::init(1, Settings::brightnessValue())) {
        while (true) delay(1000);
    }

    // Apply persisted theme and key-repeat timings.
    SettingsScene::applyTheme();
    CardGFX::input().setRepeatTimings(
        Settings::keyRepeatDelayMs(), Settings::keyRepeatRateMs());

    chessScene.setup();
    lobbyScene.setup(&chessScene, &settingsScene);
    settingsScene.setup();

    CardGFX::scenes().registerScene(&chessScene);
    CardGFX::scenes().registerScene(&lobbyScene);
    CardGFX::scenes().registerScene(&settingsScene);
    CardGFX::scenes().push(&lobbyScene);

    ChessOpeningBook::init();
    Serial.println("BOOT OK - Chess");
}

void loop() {
    CardGFX::tick();
}
