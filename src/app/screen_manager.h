/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "asset_manager.h"
#include "base_screen.h"
#include "base_viewmodel.h"
#include "lvgl.h"

#include <memory>

namespace app {

class ScreenManager {
public:
    ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets);
    ~ScreenManager();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void start();
    lv_obj_t* current_screen() const;

private:
    void load_screen(std::unique_ptr<screen::BaseScreen> next_screen);

    viewmodel::BaseViewModel& view_model_;
    AssetManager& assets_;
    std::unique_ptr<screen::BaseScreen> current_screen_;
};

} // namespace app
