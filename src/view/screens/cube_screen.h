/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"
#include "cube_view.h"

#include <memory>

namespace screen {

class CubeScreen : public BaseScreen {
public:
    CubeScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;

    std::unique_ptr<view::widgets::CubeView> cube_view_;
};

} // namespace screen
