/*
 * SPDX-License-Identifier: MIT
 */

#include "cube_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {
namespace {

constexpr int kCubeViewWidth = 150;
constexpr int kInfoWidth = 160;

} // namespace

CubeScreen::CubeScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void CubeScreen::build_content(lv_obj_t* content) {
    cube_view_ = std::make_unique<view::widgets::CubeView>(content, view_model());
    cube_view_->build();
    lv_obj_set_size(cube_view_->root(), kCubeViewWidth, LV_PCT(100));
    lv_obj_align(cube_view_->root(), LV_ALIGN_LEFT_MID, 0, 0);

    auto* info = lv_obj_create(content);
    lv_obj_remove_style_all(info);
    lv_obj_set_size(info, kInfoWidth, LV_PCT(100));
    lv_obj_align(info, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(info, 6, 0);
    lv_obj_set_style_pad_ver(info, 8, 0);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);

    auto* status_font = assets().load_standard_font(14);
    auto* moves_font = assets().load_standard_font(12, app::StandardFontWeight::Regular);

    auto* status = lv_label_create(info);
    lv_label_bind_text(status, view_model().status_subject(), nullptr);
    lv_obj_set_width(status, LV_PCT(100));
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(status, status_font ? status_font : &lv_font_montserrat_14, 0);
    reactive::bind_theme(status, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* moves = lv_label_create(info);
    lv_label_bind_text(moves, view_model().moves_subject(), nullptr);
    lv_obj_set_width(moves, LV_PCT(100));
    lv_label_set_long_mode(moves, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(moves, moves_font ? moves_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(moves, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
