/*
 * SPDX-License-Identifier: MIT
 */

#include "cube_model.h"

namespace model {

CubeModel::CubeModel() {
    rubik_goal_pattern(stickers_.data());
}

const CubeModel::Pattern& CubeModel::stickers() const {
    return stickers_;
}

bool CubeModel::is_solved() const {
    return rubik_is_solved(stickers_.data()) != 0;
}

void CubeModel::apply_move(char move) {
    rubik_apply_move(stickers_.data(), move);
}

} // namespace model
