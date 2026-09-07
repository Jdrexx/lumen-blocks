#ifndef LUMENBLOCKS_GAME_RULES_H
#define LUMENBLOCKS_GAME_RULES_H

// Pure game rules for Lumen Blocks — deliberately free of Android, EGL, GLES,
// JNI and rendering dependencies so the exact placement/clear logic can be
// unit-tested on a host machine (see tests/). Renderer.h/cpp own the state and
// delegate to these functions; do not fork the rules here and in Renderer.
//
// Board coordinate convention (matches Renderer):
//   board_[row][column], row 0 = top. A cell value is 0 (empty) or a color
//   index 1..7 (kBlockColors + 1). Piece specials: 0 normal, 1 prism (may
//   overlap occupied cells), 2 lantern (clears neighbours on placement).

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace game {

constexpr int kBoardSize = 8;

using Board = std::array<std::array<int, kBoardSize>, kBoardSize>;

struct Cell {
    int x;
    int y;
};

struct Piece {
    std::vector<Cell> cells;
    int color = 0;
    int special = 0;  // 0 = normal, 1 = prism, 2 = lantern
    bool used = false;
};

// True when every cell of `piece` lands on an in-board cell and every target
// cell is empty — or occupied by an earlier piece while the placing piece is a
// prism (special == 1), which is the one overlap the rules allow.
inline bool canPlace(const Board &board, const Piece &piece, int column, int row) {
    for (const auto &cell : piece.cells) {
        const int x = column + cell.x;
        const int y = row + cell.y;
        if (x < 0 || x >= kBoardSize || y < 0 || y >= kBoardSize ||
            (board[y][x] != 0 && piece.special != 1)) {
            return false;
        }
    }
    return true;
}

// Does any UNUSED tray piece fit somewhere on the board?
inline bool hasAnyMove(const Board &board, const std::array<Piece, 3> &tray) {
    for (const auto &piece : tray) {
        if (piece.used) continue;
        for (int row = 0; row < kBoardSize; ++row) {
            for (int column = 0; column < kBoardSize; ++column) {
                if (canPlace(board, piece, column, row)) return true;
            }
        }
    }
    return false;
}

// Scans the board for complete rows and columns. Returns how many lines are
// full and records WHICH ones in fullRows/fullColumns (callers clear those
// cells). A cell at the intersection of a full row and a full column is
// counted once per line, matching a simultaneous multi-line clear.
inline int scanFullLines(const Board &board,
                         std::array<bool, kBoardSize> &fullRows,
                         std::array<bool, kBoardSize> &fullColumns) {
    fullRows.fill(false);
    fullColumns.fill(false);
    int lineCount = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        fullRows[row] = true;
        for (int column = 0; column < kBoardSize; ++column) {
            fullRows[row] = fullRows[row] && board[row][column] != 0;
        }
        if (fullRows[row]) ++lineCount;
    }
    for (int column = 0; column < kBoardSize; ++column) {
        fullColumns[column] = true;
        for (int row = 0; row < kBoardSize; ++row) {
            fullColumns[column] = fullColumns[column] && board[row][column] != 0;
        }
        if (fullColumns[column]) ++lineCount;
    }
    return lineCount;
}

// Bounding extent of a piece (max cell coordinate + 1); a single-cell piece is
// 1x1. Used for tray layout and drag centring.
inline int pieceWidth(const Piece &piece) {
    int maximum = 0;
    for (const auto &cell : piece.cells) maximum = std::max(maximum, cell.x);
    return maximum + 1;
}

inline int pieceHeight(const Piece &piece) {
    int maximum = 0;
    for (const auto &cell : piece.cells) maximum = std::max(maximum, cell.y);
    return maximum + 1;
}

}  // namespace game

#endif  // LUMENBLOCKS_GAME_RULES_H
