/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_widget.h"
#include "base_viewmodel.h"

#include <array>

namespace app {
class AssetManager;
}

namespace view::widgets {

// Bottom bar with the keyboard shortcuts of the cube screen.
class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;

    void build() override;

private:
    void create_shortcut_hints();
    void update_key_colors();

    static void theme_observer_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<lv_obj_t*, 3> shortcut_key_labels_{};
    lv_observer_t* theme_observer_{nullptr};
};

} // namespace view::widgets
