/*
 * SPDX-License-Identifier: MIT
 */

#include "cube_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace screen {
namespace {

constexpr int kCubeViewWidth = 150;
constexpr int kInfoWidth = 160;
// Offset that moves the cube from the left edge to the horizontal centre of the screen.
constexpr int kCubeCenteredX = (view::kScreenWidth - kCubeViewWidth) / 2;
constexpr uint32_t kSlideDurationMs = 300;

void cube_x_anim_cb(void* var, int32_t value) {
    lv_obj_set_x(static_cast<lv_obj_t*>(var), value);
}

void info_opa_anim_cb(void* var, int32_t value) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(value), 0);
}

void info_faded_out_cb(lv_anim_t* anim) {
    lv_obj_add_flag(static_cast<lv_obj_t*>(anim->var), LV_OBJ_FLAG_HIDDEN);
}

void start_anim(lv_obj_t* obj,
                lv_anim_exec_xcb_t exec_cb,
                int32_t from,
                int32_t to,
                lv_anim_completed_cb_t completed_cb = nullptr) {
    lv_anim_delete(obj, exec_cb);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, exec_cb);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_duration(&anim, kSlideDurationMs);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    if (completed_cb) {
        lv_anim_set_completed_cb(&anim, completed_cb);
    }
    lv_anim_start(&anim);
}

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
    info_ = info;
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

    reactive::observe_obj(info, view_model().info_hidden_subject(), info_hidden_observer_cb, this);
}

void CubeScreen::set_info_hidden(bool hidden) {
    if (hidden == info_hidden_) {
        return;
    }
    info_hidden_ = hidden;

    auto* cube = cube_view_->root();
    const auto cube_x = lv_obj_get_x_aligned(cube);
    if (hidden) {
        start_anim(cube, cube_x_anim_cb, cube_x, kCubeCenteredX);
        start_anim(info_, info_opa_anim_cb, LV_OPA_COVER, LV_OPA_TRANSP, info_faded_out_cb);
    }
    else {
        lv_obj_remove_flag(info_, LV_OBJ_FLAG_HIDDEN);
        start_anim(cube, cube_x_anim_cb, cube_x, 0);
        start_anim(info_, info_opa_anim_cb, lv_obj_get_style_opa(info_, LV_PART_MAIN), LV_OPA_COVER);
    }
}

void CubeScreen::info_hidden_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    auto* screen = static_cast<CubeScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->set_info_hidden(lv_subject_get_int(subject) != 0);
    }
}

} // namespace screen
