#include "puzzle_storage.h"
#include <SD.h>
#include <cstring>

static constexpr const char* SAVE_DIR  = "/chess";
static constexpr const char* SAVE_PATH = "/chess/puzzles.dat";

namespace PuzzleStorage {

static bool ensureDir() {
    if (!SD.exists(SAVE_DIR)) {
        return SD.mkdir(SAVE_DIR);
    }
    return true;
}

void loadProgress(PuzzleProgress& progress) {
    memset(&progress, 0, sizeof(progress));
    if (!SD.exists(SAVE_PATH)) return;

    File f = SD.open(SAVE_PATH, FILE_READ);
    if (!f) return;
    if (f.size() >= sizeof(PuzzleProgress)) {
        f.read((uint8_t*)&progress, sizeof(PuzzleProgress));
    }
    f.close();
}

void saveProgress(const PuzzleProgress& progress) {
    ensureDir();
    File f = SD.open(SAVE_PATH, FILE_WRITE);
    if (!f) return;
    f.write((const uint8_t*)&progress, sizeof(PuzzleProgress));
    f.close();
}

bool isPuzzleCompleted(const PuzzleProgress& progress, uint8_t index) {
    return (progress.completed[index / 8] & (1 << (index % 8))) != 0;
}

void markPuzzleCompleted(PuzzleProgress& progress, uint8_t index) {
    progress.completed[index / 8] |= (1 << (index % 8));
}

uint16_t completedCount(const PuzzleProgress& progress, uint16_t total) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < total; i++) {
        if (isPuzzleCompleted(progress, (uint8_t)i)) count++;
    }
    return count;
}

} // namespace PuzzleStorage
