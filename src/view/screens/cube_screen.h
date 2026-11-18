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
    void set_info_hidden(bool hidden);

    static void info_hidden_observer_cb(lv_observer_t* observer, lv_subject_t* subject);

    std::unique_ptr<view::widgets::CubeView> cube_view_;
    lv_obj_t* info_{nullptr};
    bool info_hidden_{false};
};

} // namespace screen
