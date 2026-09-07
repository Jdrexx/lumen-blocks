// Host-side unit tests for the pure Lumen Blocks game rules (game_rules.h).
// Build & run (no Android SDK/NDK required):
//   cmake -S tests -B tests/build && cmake --build tests/build -j
//   ctest --test-dir tests/build --output-on-failure
#include "../app/src/main/cpp/game_rules.h"

#include <cassert>
#include <cstdio>
#include <string>

using game::Board;
using game::Piece;

namespace {

int g_checks = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        ++g_checks;                                                                                \
        if (!(cond)) {                                                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

Piece single(int color = 1) {
    return {{{0, 0}}, color, 0, false};
}
Piece domino() {
    return {{{0, 0}, {1, 0}}, 1, 0, false};
}
Piece square() {
    return {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}, 1, 0, false};
}
Piece prism() {
    return {{{0, 0}, {1, 0}}, 1, 1, false};
}  // may overlap
Piece lantern() {
    return {{{0, 0}, {1, 0}}, 1, 2, false};
}  // may NOT overlap
Piece line4() {
    return {{{0, 0}, {1, 0}, {2, 0}, {3, 0}}, 1, 0, false};
}

void test_canPlace_bounds() {
    Board b{};  // empty
    CHECK(game::canPlace(b, single(), 0, 0));
    CHECK(game::canPlace(b, single(), 7, 7));
    CHECK(!game::canPlace(b, single(), -1, 0));
    CHECK(!game::canPlace(b, single(), 8, 0));
    CHECK(!game::canPlace(b, single(), 0, -1));
    CHECK(!game::canPlace(b, single(), 0, 8));
    // Multi-cell pieces must land entirely in-bounds.
    Piece vertical = {{{0, 0}, {0, 1}, {0, 2}, {0, 3}}, 1, 0, false};
    CHECK(game::canPlace(b, line4(), 4, 0));
    CHECK(!game::canPlace(b, line4(), 5, 0));   // hangs off the right edge
    CHECK(!game::canPlace(b, vertical, 0, 5));  // row 8 is off-board
    CHECK(game::canPlace(b, vertical, 0, 4));   // rows 4..7 — just fits
    CHECK(!game::canPlace(b, square(), 7, 7));  // 2x2 from the corner
    CHECK(game::canPlace(b, square(), 6, 6));
}

void test_canPlace_occupancy() {
    Board b{};
    b[0][0] = 1;  // occupied
    CHECK(!game::canPlace(b, single(), 0, 0));
    CHECK(game::canPlace(b, single(), 0, 1));  // adjacent free cell is fine
    // A normal piece may not overlap ANY occupied cell.
    b[0][1] = 2;
    CHECK(!game::canPlace(b, domino(), 0, 0));
    // Prism pieces may overlap occupied cells...
    CHECK(game::canPlace(b, prism(), 0, 0));
    // ...but lanterns (special == 2) may NOT — only special == 1 overlaps.
    CHECK(!game::canPlace(b, lantern(), 0, 0));
    // Prisms still respect the board bounds.
    CHECK(!game::canPlace(b, prism(), -1, 0));
}

void test_piece_extents() {
    CHECK(game::pieceWidth(single()) == 1);
    CHECK(game::pieceHeight(single()) == 1);
    CHECK(game::pieceWidth(domino()) == 2);
    CHECK(game::pieceHeight(domino()) == 1);
    CHECK(game::pieceWidth(square()) == 2);
    CHECK(game::pieceHeight(square()) == 2);
    Piece vertical = {{{0, 0}, {0, 1}, {0, 2}, {0, 3}}, 1, 0, false};
    CHECK(game::pieceWidth(vertical) == 1);
    CHECK(game::pieceHeight(vertical) == 4);
}

void test_scanFullLines() {
    // Empty board: nothing full.
    Board b{};
    std::array<bool, game::kBoardSize> rows{}, cols{};
    CHECK(game::scanFullLines(b, rows, cols) == 0);

    // One complete row (row 0).
    for (int c = 0; c < game::kBoardSize; ++c) b[0][c] = 1;
    CHECK(game::scanFullLines(b, rows, cols) == 1);
    CHECK(rows[0] && !rows[1] && !rows[7]);
    CHECK(!cols[0] && !cols[7]);

    // One complete column (column 3) on top.
    Board b2{};
    for (int r = 0; r < game::kBoardSize; ++r) b2[r][3] = 5;
    CHECK(game::scanFullLines(b2, rows, cols) == 1);
    CHECK(cols[3] && !cols[0] && !cols[7]);
    CHECK(!rows[0] && !rows[7]);

    // Two rows + one column simultaneously.
    Board b3{};
    for (int c = 0; c < game::kBoardSize; ++c) {
        b3[1][c] = 2;
        b3[4][c] = 2;
    }
    for (int r = 0; r < game::kBoardSize; ++r) b3[r][6] = 3;
    CHECK(game::scanFullLines(b3, rows, cols) == 3);
    CHECK(rows[1] && rows[4] && !rows[0]);
    CHECK(cols[6] && !cols[0]);

    // A full board clears every row AND every column (16 lines).
    Board b4{};
    for (auto &row : b4) row.fill(4);
    CHECK(game::scanFullLines(b4, rows, cols) == 16);
    CHECK((rows ==
           std::array<bool, game::kBoardSize>{true, true, true, true, true, true, true, true}));
    CHECK((cols ==
           std::array<bool, game::kBoardSize>{true, true, true, true, true, true, true, true}));

    // Nearly-full row with one hole is NOT a clear.
    Board b5{};
    for (int c = 0; c < game::kBoardSize; ++c) b5[0][c] = 1;
    b5[0][4] = 0;
    CHECK(game::scanFullLines(b5, rows, cols) == 0);
    CHECK(!rows[0]);
}

void test_hasAnyMove() {
    // Empty board: any unused piece fits.
    Board b{};
    CHECK(game::hasAnyMove(b, {single(), single(), single()}));
    // A 2x2 tray always fits on an empty 8x8.
    CHECK(game::hasAnyMove(b, {square(), square(), square()}));

    // Full board with exactly one free cell: a single-cell piece fits, a
    // 2x2 square does not.
    Board b2{};
    for (auto &row : b2) row.fill(1);
    b2[0][0] = 0;
    CHECK(game::hasAnyMove(b2, {single(), single(), single()}));
    CHECK(!game::hasAnyMove(b2, {square(), square(), square()}));

    // Used tray pieces are ignored: only unused ones count.
    Piece usedSingle = single();
    usedSingle.used = true;
    CHECK(!game::hasAnyMove(b2, {square(), square(), usedSingle}));
    CHECK(game::hasAnyMove(b2, {square(), square(), single()}));
}

}  // namespace

int main() {
    test_canPlace_bounds();
    test_canPlace_occupancy();
    test_piece_extents();
    test_scanFullLines();
    test_hasAnyMove();
    std::printf("ALL %d CHECKS PASSED\n", g_checks);
    return 0;
}
