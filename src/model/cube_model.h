/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "rubik_solver.h"

#include <array>

namespace model {

// State of the 2x2x2 cube: same 24-sticker pattern as `quadValues` in 3d/main.c.
class CubeModel {
public:
    using Pattern = std::array<char, RUBIK_STICKER_COUNT>;

    CubeModel();

    const Pattern& stickers() const;
    bool is_solved() const;

    // move is one of x X y Y z Z
    void apply_move(char move);

private:
    Pattern stickers_{};
};

} // namespace model
