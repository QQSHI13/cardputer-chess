#include "chess_ai.h"
#include "chess_opening_book.h"
#include <Arduino.h>
#include <esp_random.h>

// =====================================================================
// Piece-square tables (from White's perspective, row 0 = rank 1).
// Values are centipawn bonuses added to material score.
// =====================================================================

static const int8_t PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int8_t PST_KNIGHT[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};

static const int8_t PST_BISHOP[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};

static const int8_t PST_ROOK[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int8_t PST_QUEEN[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -10,  5,  5,  5,  5,  5,  0,-10,
     0,  0,  5,  5,  5,  5,  0, -5,
    -5,  0,  5,  5,  5,  5,  0, -5,
   -10,  0,  5,  5,  5,  5,  0,-10,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};

static const int8_t PST_KING_MID[64] = {
    20, 30, 10,  0,  0, 10, 30, 20,
    20, 20,  0,  0,  0,  0, 20, 20,
   -10,-20,-20,-20,-20,-20,-20,-10,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30
};

// Endgame king: encourage centralization and active king. Used once the
// total non-king material drops below ENDGAME_MATERIAL.
static const int8_t PST_KING_END[64] = {
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -50,-40,-30,-20,-20,-30,-40,-50
};

// Total non-king material (centipawns) below which we treat the position
// as an endgame for king PST selection.
static constexpr int16_t ENDGAME_MATERIAL = 1300;

// ── Helpers ──────────────────────────────────────────────────────────

static const int16_t MATERIAL[] = {
    0,    // None
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    0     // King (not counted in material)
};

static int8_t pstValue(PieceType type, uint8_t col, uint8_t row, PieceColor color,
                        bool kingEndgame) {
    // Tables are from White's perspective; mirror row for Black
    uint8_t r = (color == PieceColor::White) ? row : (7 - row);
    uint8_t idx = r * 8 + col;

    switch (type) {
    case PieceType::Pawn:   return PST_PAWN[idx];
    case PieceType::Knight: return PST_KNIGHT[idx];
    case PieceType::Bishop: return PST_BISHOP[idx];
    case PieceType::Rook:   return PST_ROOK[idx];
    case PieceType::Queen:  return PST_QUEEN[idx];
    case PieceType::King:   return kingEndgame ? PST_KING_END[idx] : PST_KING_MID[idx];
    default: return 0;
    }
}

// ── Evaluation ───────────────────────────────────────────────────────

int16_t ChessAI::evaluate(const ChessBoard& board) {
    int16_t score = 0;
    int16_t nonKingMaterial = 0;

    // First pass: material + decide endgame phase
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            Piece p = board.at(col, row);
            if (p.empty() || p.type == PieceType::King) continue;
            nonKingMaterial += MATERIAL[static_cast<uint8_t>(p.type)];
        }
    }
    bool kingEndgame = (nonKingMaterial < ENDGAME_MATERIAL);

    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            Piece p = board.at(col, row);
            if (p.empty()) continue;

            int16_t val = MATERIAL[static_cast<uint8_t>(p.type)]
                        + pstValue(p.type, col, row, p.color, kingEndgame);

            if (p.color == PieceColor::White) {
                score += val;
            } else {
                score -= val;
            }
        }
    }

    // Return from perspective of side to move
    return (board.sideToMove() == PieceColor::White) ? score : -score;
}

// ── Move Ordering ────────────────────────────────────────────────────
// Score moves for ordering: captures and promotions first for better pruning.

static int16_t scoreMoveForOrdering(const Move& move, const ChessBoard& board) {
    int16_t score = 0;

    // Promotions are very valuable
    if (move.promotion != PieceType::None) {
        score += 800 + MATERIAL[static_cast<uint8_t>(move.promotion)];
    }

    // MVV-LVA for captures: prioritize capturing high-value pieces with low-value attackers
    Piece captured = board.at(move.to.col, move.to.row);
    if (!captured.empty()) {
        Piece attacker = board.at(move.from.col, move.from.row);
        score += MATERIAL[static_cast<uint8_t>(captured.type)] * 10
               - MATERIAL[static_cast<uint8_t>(attacker.type)];
    }

    // En passant capture
    if (move.isEnPassant) {
        score += 1000; // Worth a pawn capture
    }

    return score;
}

static void sortMoves(MoveList& moves, const ChessBoard& board) {
    // Pre-compute scores then insertion sort (move list is typically <40 moves)
    int16_t scores[MoveList::MAX_MOVES];
    for (uint8_t i = 0; i < moves.count; i++) {
        scores[i] = scoreMoveForOrdering(moves.moves[i], board);
    }
    for (uint8_t i = 1; i < moves.count; i++) {
        Move key = moves.moves[i];
        int16_t keyScore = scores[i];
        int8_t j = i - 1;
        while (j >= 0 && scores[j] < keyScore) {
            moves.moves[j + 1] = moves.moves[j];
            scores[j + 1] = scores[j];
            j--;
        }
        moves.moves[j + 1] = key;
        scores[j + 1] = keyScore;
    }
}

// ── Search ───────────────────────────────────────────────────────────

static uint32_t s_searchDeadline = 0;
static bool     s_searchAborted  = false;
static uint32_t s_nodeCount      = 0;

// Poll the clock every N nodes instead of every node — millis() is cheap
// but call overhead matters in the hot inner loop. N is small enough that
// we never overshoot the deadline by more than a fraction of a second even
// in the deepest quiescence explosions.
static constexpr uint32_t NODES_PER_TIME_CHECK = 1024;

static inline void checkTime() {
    if (s_nodeCount++ % NODES_PER_TIME_CHECK == 0 &&
        millis() > s_searchDeadline) {
        s_searchAborted = true;
    }
}

static constexpr int MAX_QUIESCE_DEPTH = 8;

// Quiescence search: only consider captures to avoid horizon effect
static int16_t quiesce(ChessBoard& board, int16_t alpha, int16_t beta, int qdepth) {
    if (s_searchAborted) return 0;
    checkTime();
    if (s_searchAborted) return 0;

    int16_t standPat = ChessAI::evaluate(board);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    if (qdepth >= MAX_QUIESCE_DEPTH) return alpha;

    MoveList moves;
    ChessRules::generateLegal(board, moves);

    for (uint8_t i = 0; i < moves.count; i++) {
        const Move& m = moves.moves[i];

        // Only consider captures and promotions
        bool isCapture = !board.at(m.to.col, m.to.row).empty() || m.isEnPassant;
        if (!isCapture && m.promotion == PieceType::None) continue;

        MoveRecord rec = board.makeMove(m);
        int16_t score = -quiesce(board, -beta, -alpha, qdepth + 1);
        board.unmakeMove(rec);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int16_t alphaBeta(ChessBoard& board, int depth, int16_t alpha, int16_t beta) {
    if (s_searchAborted) return 0;
    checkTime();
    if (s_searchAborted) return 0;

    if (depth <= 0) {
        return quiesce(board, alpha, beta, 0);
    }

    MoveList moves;
    ChessRules::generateLegal(board, moves);

    // No legal moves: checkmate or stalemate
    if (moves.count == 0) {
        if (ChessRules::isInCheck(board, board.sideToMove())) {
            return -10000 + (6 - depth); // Prefer faster checkmates
        }
        return 0; // Stalemate
    }

    // Draw detection
    if (board.halfmoveClock() >= 100) return 0; // 50-move rule

    sortMoves(moves, board);

    for (uint8_t i = 0; i < moves.count; i++) {
        if (s_searchAborted) return 0;

        MoveRecord rec = board.makeMove(moves.moves[i]);
        int16_t score = -alphaBeta(board, depth - 1, -beta, -alpha);
        board.unmakeMove(rec);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// ── Public API ───────────────────────────────────────────────────────

Move ChessAI::findBestMove(ChessBoard& board, AIDifficulty difficulty, bool useBook) {
    // Try opening book first (standard position only), unless disabled.
    if (useBook && board.variant() != ChessVariant::Chess960) {
        Move bookMove;
        if (ChessOpeningBook::probe(board, bookMove)) {
            return bookMove;
        }
    }

    int maxDepth;
    uint32_t maxTimeMs;

    switch (difficulty) {
    case AIDifficulty::Easy:   maxDepth = 2; maxTimeMs = 200;  break;
    case AIDifficulty::Medium: maxDepth = 4; maxTimeMs = 1000; break;
    case AIDifficulty::Hard:   maxDepth = 6; maxTimeMs = 3000; break;
    default:                   maxDepth = 3; maxTimeMs = 500;  break;
    }

    s_searchDeadline = millis() + maxTimeMs;
    s_searchAborted = false;
    s_nodeCount = 0;

    MoveList moves;
    ChessRules::generateLegal(board, moves);

    if (moves.count == 0) {
        // Should not happen — caller should check game end first
        return Move{};
    }

    // Single legal move — return immediately
    if (moves.count == 1) {
        return moves.moves[0];
    }

    sortMoves(moves, board);

    Move bestMove = moves.moves[0];
    int16_t bestScore = -20000;

    // Iterative deepening
    for (int depth = 1; depth <= maxDepth; depth++) {
        if (s_searchAborted) break;

        Move iterBest = moves.moves[0];
        int16_t iterBestScore = -20000;

        for (uint8_t i = 0; i < moves.count; i++) {
            if (s_searchAborted) break;

            MoveRecord rec = board.makeMove(moves.moves[i]);
            int16_t score = -alphaBeta(board, depth - 1, -20000, -iterBestScore);
            board.unmakeMove(rec);

            if (!s_searchAborted && score > iterBestScore) {
                iterBestScore = score;
                iterBest = moves.moves[i];
            }
        }

        // Only update best if this iteration completed (or at least found something)
        if (!s_searchAborted || depth == 1) {
            bestMove = iterBest;
            bestScore = iterBestScore;
        }
    }

    // Easy mode: 30% chance to pick a random legal move instead of the best
    if (difficulty == AIDifficulty::Easy && moves.count > 1) {
        if ((esp_random() % 100) < 30) {
            uint8_t idx = esp_random() % moves.count;
            bestMove = moves.moves[idx];
        }
    }

    return bestMove;
}
