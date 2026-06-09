#include <Arduino.h>
#include <cardgfx.h>
#include <SD.h>
#include "chess_scene.h"
#include "lobby_scene.h"
#include "chess_opening_book.h"

using namespace CardGFX;

static ChessScene chessScene;
static LobbyScene lobbyScene;

// =====================================================================
// Setup & Loop
// =====================================================================

void setup() {
    // Mount SD card before UI init so storage layers can write immediately.
    if (!SD.begin(4)) {
        Serial.println("WARN: SD card mount failed — saves will not persist");
    } else {
        Serial.println("SD card mounted");
    }

    if (!CardGFX::init(1, 128)) {
        while (true) delay(1000);
    }

    chessScene.setup();
    lobbyScene.setup(&chessScene);

    CardGFX::scenes().registerScene(&chessScene);
    CardGFX::scenes().registerScene(&lobbyScene);
    CardGFX::scenes().push(&lobbyScene);

    ChessOpeningBook::init();
    Serial.println("BOOT OK - Chess ADV");
}

void loop() {
    CardGFX::tick();
}
