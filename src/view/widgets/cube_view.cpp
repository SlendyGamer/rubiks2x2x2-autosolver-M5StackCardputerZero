/*
 * SPDX-License-Identifier: MIT
 */

#include "cube_view.h"

#include "bindings.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace view::widgets {
namespace {

constexpr int kInnerColorIndex = 24;

// Color of each sticker label, same table as `colors` in 3d/main.c (24 = gray inner faces).
lv_color_t sticker_color(int label) {
    switch (label) {
        case 1: case 4: case 17: case 18:
            return lv_color_make(255, 255, 255); // white
        case 6: case 9: case 12: case 21:
            return lv_color_make(255, 255, 0);   // yellow
        case 5: case 8: case 20: case 22:
            return lv_color_make(255, 128, 0);   // orange
        case 0: case 10: case 14: case 15:
            return lv_color_make(255, 0, 0);     // red
        case 2: case 3: case 7: case 11:
            return lv_color_make(0, 0, 255);     // blue
        case 13: case 16: case 19: case 23:
            return lv_color_make(0, 255, 0);     // green
        default:
            return lv_color_make(89, 89, 89);    // gray
    }
}

constexpr float kPi = 3.14159265358979f;
constexpr float kCameraDistance = 5.5f;   // glTranslatef(0, 0, -5) in 3d/main.c
constexpr float kFocalLength = 125.0f;    // pixels, tuned so the cube fits the 150x110 view in any orientation
constexpr float kStickerInset = 0.88f;    // visible part of each face, the rest is the cube body
constexpr uint32_t kBodyColor = 0x1a1a1a;

struct Vec3 {
    float x;
    float y;
    float z;

    float& at(int axis) { return axis == 0 ? x : (axis == 1 ? y : z); }
    float at(int axis) const { return axis == 0 ? x : (axis == 1 ? y : z); }
};

Vec3 rotate_about(Vec3 v, int axis, float degrees) {
    const float rad = degrees * kPi / 180.0f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    switch (axis) {
        case 0:
            return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
        case 1:
            return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
        default:
            return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
    }
}

// Index (0..23) of the sticker on the outer face (axis, sign) of the cubie at (sx, sy, sz).
// Same layout as the quads drawn in drawCube() of 3d/main.c.
int sticker_index(int axis, int sign, int sx, int sy, int sz) {
    if (axis == 2 && sign > 0) {
        return (sy > 0 ? 2 : 0) + (sx > 0 ? 1 : 0);
    }
    if (axis == 2) {
        return 4 + (sy > 0 ? 2 : 0) + (sx > 0 ? 0 : 1);
    }
    if (axis == 0 && sign < 0) {
        return 8 + (sy > 0 ? 2 : 0) + (sz > 0 ? 1 : 0);
    }
    if (axis == 0) {
        return 12 + (sy > 0 ? 2 : 0) + (sz > 0 ? 0 : 1);
    }
    if (axis == 1 && sign > 0) {
        return 16 + (sz > 0 ? 0 : 2) + (sx > 0 ? 1 : 0);
    }
    return 20 + (sz > 0 ? 2 : 0) + (sx > 0 ? 1 : 0);
}

struct Face {
    float depth;
    std::array<lv_point_precise_t, 4> body;
    std::array<lv_point_precise_t, 4> sticker;
    int color_index;
    bool outer;
};

int turning_axis(char move) {
    switch (move) {
        case 'x': case 'X': return 0;
        case 'y': case 'Y': return 1;
        case 'z': case 'Z': return 2;
        default: return -1;
    }
}

// The quad is split in 4 overlapping triangles so antialiasing leaves no seam on the diagonals.
void fill_quad(lv_layer_t* layer, const std::array<lv_point_precise_t, 4>& quad, lv_color_t color) {
    static constexpr int kTriangles[4][3] = {{0, 1, 2}, {0, 2, 3}, {0, 1, 3}, {1, 2, 3}};

    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    for (const auto& triangle : kTriangles) {
        for (int i = 0; i < 3; ++i) {
            dsc.p[i] = quad[triangle[i]];
        }
        lv_draw_triangle(layer, &dsc);
    }
}

} // namespace

CubeView::CubeView(lv_obj_t* parent, viewmodel::BaseViewModel& view_model)
    : BaseWidgets(parent), view_model_(view_model) {}

void CubeView::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(core_obj_, draw_event_cb, LV_EVENT_DRAW_MAIN, this);
    reactive::observe_obj(core_obj_, view_model_.cube_revision_subject(), revision_observer_cb, this);
}

void CubeView::draw(lv_layer_t* layer) {
    lv_area_t area;
    lv_obj_get_coords(core_obj_, &area);
    const float center_x = static_cast<float>(area.x1) + static_cast<float>(lv_area_get_width(&area)) / 2.0f;
    const float center_y = static_cast<float>(area.y1) + static_cast<float>(lv_area_get_height(&area)) / 2.0f;

    const auto& stickers = view_model_.cube().stickers();
    const char turning = view_model_.turning_move();
    const int layer_axis = turning_axis(turning);
    const float layer_angle = (turning == 'X' || turning == 'Y' || turning == 'Z')
                                  ? -view_model_.turning_angle()
                                  : view_model_.turning_angle();
    const float pitch = view_model_.camera_pitch();
    const float yaw = view_model_.camera_yaw();

    // Same order as the glRotatef calls in display(): yaw (angleY), then pitch (angleX).
    const auto to_camera = [&](Vec3 p, bool in_turning_layer, bool is_point) {
        if (in_turning_layer) {
            p = rotate_about(p, layer_axis, layer_angle);
        }
        p = rotate_about(p, 1, yaw);
        p = rotate_about(p, 0, pitch);
        if (is_point) {
            p.z -= kCameraDistance;
        }
        return p;
    };
    const auto project = [&](Vec3 p) {
        const float depth = std::max(-p.z, 0.1f);
        return lv_point_precise_t{static_cast<lv_value_precise_t>(center_x + kFocalLength * p.x / depth),
                                  static_cast<lv_value_precise_t>(center_y - kFocalLength * p.y / depth)};
    };

    std::array<Face, 48> faces;
    size_t face_count = 0;

    // 8 cubies (corners), each spanning [0, 1] or [-1, 0] on every axis.
    for (int cubie = 0; cubie < 8; ++cubie) {
        const std::array<int, 3> s = {(cubie & 1) ? 1 : -1, (cubie & 2) ? 1 : -1, (cubie & 4) ? 1 : -1};
        const bool in_turning_layer = layer_axis >= 0 && s[layer_axis] > 0;

        for (int axis = 0; axis < 3; ++axis) {
            for (int sign = -1; sign <= 1; sign += 2) {
                const bool outer = s[axis] == sign;
                const int u = (axis + 1) % 3;
                const int v = (axis + 2) % 3;
                const float u_lo = s[u] > 0 ? 0.0f : -1.0f;
                const float v_lo = s[v] > 0 ? 0.0f : -1.0f;

                std::array<Vec3, 4> corners{};
                const std::array<std::array<float, 2>, 4> uv = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
                Vec3 center{0, 0, 0};
                for (int i = 0; i < 4; ++i) {
                    corners[i].at(axis) = outer ? static_cast<float>(sign) : 0.0f;
                    corners[i].at(u) = u_lo + uv[i][0];
                    corners[i].at(v) = v_lo + uv[i][1];
                    center.x += corners[i].x / 4.0f;
                    center.y += corners[i].y / 4.0f;
                    center.z += corners[i].z / 4.0f;
                }

                Vec3 normal{0, 0, 0};
                normal.at(axis) = static_cast<float>(sign);
                const Vec3 center_cam = to_camera(center, in_turning_layer, true);
                const Vec3 normal_cam = to_camera(normal, in_turning_layer, false);
                const float facing = -(normal_cam.x * center_cam.x + normal_cam.y * center_cam.y +
                                       normal_cam.z * center_cam.z);
                if (facing <= 0.0f) {
                    continue;
                }

                Face& face = faces[face_count++];
                face.depth = center_cam.z;
                face.outer = outer;
                face.color_index = outer ? stickers[sticker_index(axis, sign, s[0], s[1], s[2])]
                                         : kInnerColorIndex;
                for (int i = 0; i < 4; ++i) {
                    face.body[i] = project(to_camera(corners[i], in_turning_layer, true));
                    Vec3 inset{center.x + (corners[i].x - center.x) * kStickerInset,
                               center.y + (corners[i].y - center.y) * kStickerInset,
                               center.z + (corners[i].z - center.z) * kStickerInset};
                    face.sticker[i] = project(to_camera(inset, in_turning_layer, true));
                }
            }
        }
    }

    // Painter's algorithm: farthest faces first.
    std::sort(faces.begin(), faces.begin() + static_cast<std::ptrdiff_t>(face_count),
              [](const Face& a, const Face& b) { return a.depth < b.depth; });

    for (size_t i = 0; i < face_count; ++i) {
        const Face& face = faces[i];
        if (face.outer) {
            fill_quad(layer, face.body, lv_color_hex(kBodyColor));
            fill_quad(layer, face.sticker, sticker_color(face.color_index));
        }
        else {
            fill_quad(layer, face.body, sticker_color(kInnerColorIndex));
        }
    }
}

void CubeView::draw_event_cb(lv_event_t* event) {
    auto* view = static_cast<CubeView*>(lv_event_get_user_data(event));
    auto* layer = lv_event_get_layer(event);
    if (view && layer) {
        view->draw(layer);
    }
}

void CubeView::revision_observer_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* view = static_cast<CubeView*>(lv_observer_get_user_data(observer));
    if (view && view->core_obj_) {
        lv_obj_invalidate(view->core_obj_);
    }
}

} // namespace view::widgets
