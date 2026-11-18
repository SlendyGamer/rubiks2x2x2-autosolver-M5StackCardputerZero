/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace view::widgets {
namespace {

struct ShortcutSpec {
    const char* key;
    const char* action;
};

constexpr std::array<ShortcutSpec, 4> kShortcuts = {{
    {"X/Y/Z", "Turn"},
    {"B/D", "Search"},
    {"S", "Solve"},
    {"H", "Hide"},
}};

} // namespace

NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

NavBar::~NavBar() {
    if (theme_observer_) {
        lv_observer_remove(theme_observer_);
        theme_observer_ = nullptr;
    }
}

void NavBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);
    // The Bar theme role resets the padding, so it is set afterwards.
    lv_obj_set_style_pad_hor(core_obj_, 8, 0);

    create_shortcut_hints();
    theme_observer_ = reactive::observe_obj(core_obj_, view_model_.dark_mode_subject(), theme_observer_cb, this);
    update_key_colors();
}

void NavBar::create_shortcut_hints() {
    const lv_font_t* key_font = assets_.load_standard_font(14);
    if (!key_font) {
        key_font = &lv_font_montserrat_14;
    }
    const lv_font_t* text_font = assets_.load_standard_font(14, app::StandardFontWeight::Regular);
    if (!text_font) {
        text_font = &lv_font_montserrat_14;
    }

    for (size_t index = 0; index < kShortcuts.size(); ++index) {
        const auto& shortcut = kShortcuts[index];
        auto* item = lv_obj_create(core_obj_);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_PCT(100));
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(item, 4, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        auto* key = lv_label_create(item);
        lv_label_set_text(key, shortcut.key);
        lv_obj_set_style_text_font(key, key_font, 0);
        shortcut_key_labels_[index] = key;

        auto* label = lv_label_create(item);
        lv_label_set_text(label, shortcut.action);
        lv_obj_set_style_text_font(label, text_font, 0);
        reactive::bind_theme(label, view_model_.dark_mode_subject(), reactive::ThemeRole::Text);
    }
}

void NavBar::update_key_colors() {
    const auto colors = view::palette(view_model_.is_dark_mode());
    for (auto* key_label : shortcut_key_labels_) {
        if (key_label) {
            lv_obj_set_style_text_color(key_label, colors.primary, 0);
        }
    }
}

void NavBar::theme_observer_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* nav_bar = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav_bar) {
        nav_bar->update_key_colors();
    }
}

} // namespace view::widgets
