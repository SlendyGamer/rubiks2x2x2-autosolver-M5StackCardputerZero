/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_viewmodel.h"
#include "base_widget.h"

namespace view::widgets {

// Software 3D renderer for the 2x2x2 cube. Replaces the OpenGL drawCube() of 3d/main.c:
// same stickers, colors, layer turn animation and camera, drawn with LVGL triangles.
class CubeView : public BaseWidgets {
public:
    CubeView(lv_obj_t* parent, viewmodel::BaseViewModel& view_model);

    void build() override;

private:
    void draw(lv_layer_t* layer);

    static void draw_event_cb(lv_event_t* event);
    static void revision_observer_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
};

} // namespace view::widgets
