/*
 * SPDX-License-Identifier: MIT
 */

#include "cube_view.h"

#include "bindings.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace view::widgets {
namespace {

struct Pt {
    float x;
    float y;
};

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
    std::array<Pt, 4> body;
    std::array<Pt, 4> sticker;
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

// lv_draw_triangle() snaps the vertices to whole pixels, which makes the cube edges crooked,
// so the convex quads are rasterized here with sub-pixel vertices into an ARGB8888 buffer.
// Coverage is exact horizontally and sampled kSubScans times per pixel row vertically.
constexpr int kSubScans = 8;

class Raster {
public:
    explicit Raster(lv_draw_buf_t* buf)
        : buf_(buf),
          width_(static_cast<int>(buf->header.w)),
          height_(static_cast<int>(buf->header.h)),
          cover_(static_cast<size_t>(width_) + 2),
          span_(static_cast<size_t>(width_) + 2) {}

    void fill(const std::array<Pt, 4>& quad, lv_color_t color) {
        float min_y = quad[0].y;
        float max_y = quad[0].y;
        for (const Pt& p : quad) {
            min_y = std::min(min_y, p.y);
            max_y = std::max(max_y, p.y);
        }
        const int first = std::max(0, static_cast<int>(std::floor(min_y)));
        const int last = std::min(height_ - 1, static_cast<int>(std::floor(max_y)));

        for (int row = first; row <= last; ++row) {
            if (accumulate_row(quad, row)) {
                blend_row(row, color);
            }
        }
    }

private:
    // Fills cover_ with the partially covered pixels and span_ with the +/- steps of the fully covered run.
    bool accumulate_row(const std::array<Pt, 4>& quad, int row) {
        constexpr float kWeight = 1.0f / kSubScans;
        std::fill(cover_.begin(), cover_.end(), 0.0f);
        std::fill(span_.begin(), span_.end(), 0.0f);
        bool touched = false;

        for (int s = 0; s < kSubScans; ++s) {
            const float y = static_cast<float>(row) + (static_cast<float>(s) + 0.5f) / kSubScans;
            float lo = INFINITY;
            float hi = -INFINITY;
            for (int i = 0; i < 4; ++i) {
                const Pt& a = quad[i];
                const Pt& b = quad[(i + 1) % 4];
                if ((a.y <= y) == (b.y <= y)) {
                    continue;
                }
                const float x = a.x + (y - a.y) / (b.y - a.y) * (b.x - a.x);
                lo = std::min(lo, x);
                hi = std::max(hi, x);
            }
            lo = std::max(lo, 0.0f);
            hi = std::min(hi, static_cast<float>(width_));
            if (!(hi > lo)) {
                continue;
            }

            touched = true;
            const int il = static_cast<int>(lo);
            const int ir = static_cast<int>(hi);
            if (il == ir) {
                cover_[il] += (hi - lo) * kWeight;
                continue;
            }
            cover_[il] += (static_cast<float>(il + 1) - lo) * kWeight;
            cover_[ir] += (hi - static_cast<float>(ir)) * kWeight;
            if (ir > il + 1) {
                span_[il + 1] += kWeight;
                span_[ir] -= kWeight;
            }
        }
        return touched;
    }

    void blend_row(int row, lv_color_t color) {
        auto* px = reinterpret_cast<lv_color32_t*>(static_cast<uint8_t*>(buf_->data) +
                                                   static_cast<size_t>(row) * buf_->header.stride);
        float run = 0.0f;
        for (int x = 0; x < width_; ++x) {
            run += span_[x];
            const float c = cover_[x] + run;
            if (c < 0.004f) {
                continue;
            }
            if (c >= 0.996f) {
                px[x].blue = color.blue;
                px[x].green = color.green;
                px[x].red = color.red;
                px[x].alpha = 255;
                continue;
            }
            const float dst_weight = (static_cast<float>(px[x].alpha) / 255.0f) * (1.0f - c);
            const float out_alpha = c + dst_weight;
            const auto mix = [&](uint8_t src, uint8_t dst) {
                return static_cast<uint8_t>((static_cast<float>(src) * c + static_cast<float>(dst) * dst_weight) /
                                                out_alpha + 0.5f);
            };
            px[x].blue = mix(color.blue, px[x].blue);
            px[x].green = mix(color.green, px[x].green);
            px[x].red = mix(color.red, px[x].red);
            px[x].alpha = static_cast<uint8_t>(out_alpha * 255.0f + 0.5f);
        }
    }

    lv_draw_buf_t* buf_;
    int width_;
    int height_;
    std::vector<float> cover_;
    std::vector<float> span_;
};

} // namespace

CubeView::CubeView(lv_obj_t* parent, viewmodel::BaseViewModel& view_model)
    : BaseWidgets(parent), view_model_(view_model) {}

CubeView::~CubeView() {
    if (canvas_) {
        lv_draw_buf_destroy(canvas_);
    }
}

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
    const int width = lv_area_get_width(&area);
    const int height = lv_area_get_height(&area);
    if (width <= 0 || height <= 0) {
        return;
    }
    if (!canvas_ || static_cast<int>(canvas_->header.w) != width || static_cast<int>(canvas_->header.h) != height) {
        if (canvas_) {
            lv_draw_buf_destroy(canvas_);
        }
        canvas_ = lv_draw_buf_create(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                     LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
        if (!canvas_) {
            return;
        }
    }
    lv_draw_buf_clear(canvas_, nullptr);

    const float center_x = static_cast<float>(width) / 2.0f;
    const float center_y = static_cast<float>(height) / 2.0f;

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
        return Pt{center_x + kFocalLength * p.x / depth, center_y - kFocalLength * p.y / depth};
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

    Raster raster(canvas_);
    for (size_t i = 0; i < face_count; ++i) {
        const Face& face = faces[i];
        if (face.outer) {
            raster.fill(face.body, lv_color_hex(kBodyColor));
            raster.fill(face.sticker, sticker_color(face.color_index));
        }
        else {
            raster.fill(face.body, sticker_color(kInnerColorIndex));
        }
    }

    // The buffer is rewritten on every draw, so drop any decoded copy cached for it.
    lv_image_cache_drop(canvas_);
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = canvas_;
    lv_draw_image(layer, &image, &area);
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
