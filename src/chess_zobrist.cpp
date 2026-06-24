#include "chess_zobrist.h"
#include <pgmspace.h>

// =====================================================================
// Zobrist hashing for position identification.
//
// The random tables are generated at compile time with a constexpr
// Galois LFSR (same taps as before: 32, 22, 2, 1) and live in flash
// (.rodata) via `constexpr` storage, so they consume no SRAM. Reads
// go through pgm_read_dword so the values are usable from any code
// path (flash-mapped on ESP32, but pgm_read is the documented API).
// =====================================================================

namespace {

// 32-bit Galois LFSR with taps at bits 32, 22, 2, 1.
// Deterministic given a seed — identical sequence to the previous
// runtime generator, so existing opening-book hashes are unchanged.
constexpr uint32_t lfsrNext(uint32_t state) {
    uint32_t bit = ((state >> 0) ^ (state >> 1) ^ (state >> 21) ^ (state >> 31)) & 1u;
    return (state >> 1) | (bit << 31);
}

// Build a table of `count` LFSR outputs starting from `seed`, returning
// the final state so callers can chain generators.
template <uint32_t Count>
struct Table {
    uint32_t data[Count] = {};
    uint32_t finalState = 0;
};

template <uint32_t Count>
constexpr Table<Count> buildTable(uint32_t seed) {
    Table<Count> t{};
    uint32_t state = seed;
    for (uint32_t i = 0; i < Count; i++) {
        state = lfsrNext(state);
        t.data[i] = state;
    }
    t.finalState = state;
    return t;
}

// pieceIndex: 0-11 (6 types x 2 colors: WP=0, BP=1, WN=2, BN=3, ...)
constexpr int pieceIndex(PieceType type, PieceColor color) {
    return (static_cast<int>(type) - 1) * 2 + static_cast<int>(color);
}

constexpr uint32_t PIECE_TABLE_COUNT = 12u * 64u;

constexpr auto PIECE_TABLE = buildTable<PIECE_TABLE_COUNT>(0x12345678u);
constexpr auto SIDE_TABLE  = buildTable<1>(PIECE_TABLE.finalState);
constexpr auto CASTLE_TABLE = buildTable<16>(SIDE_TABLE.finalState);
constexpr auto EP_TABLE    = buildTable<8>(CASTLE_TABLE.finalState);

} // namespace

namespace ChessZobrist {

uint32_t hash(const ChessBoard& board) {
    uint32_t h = 0;

    for (uint8_t r = 0; r < 8; r++) {
        for (uint8_t c = 0; c < 8; c++) {
            Piece p = board.at(c, r);
            if (!p.empty()) {
                int idx = pieceIndex(p.type, p.color);
                h ^= pgm_read_dword(&PIECE_TABLE.data[idx * 64 + r * 8 + c]);
            }
        }
    }

    if (board.sideToMove() == PieceColor::Black) {
        h ^= pgm_read_dword(&SIDE_TABLE.data[0]);
    }

    h ^= pgm_read_dword(&CASTLE_TABLE.data[board.castleRights() & 0x0F]);

    Square ep = board.enPassantTarget();
    if (!isNoSquare(ep)) {
        h ^= pgm_read_dword(&EP_TABLE.data[ep.col]);
    }

    return h;
}

} // namespace ChessZobrist
