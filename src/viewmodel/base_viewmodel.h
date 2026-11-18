/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"
#include "cube_model.h"

#include "lvgl.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <thread>

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* quit_requested_subject();
    // Incremented whenever the cube (state, turn animation or camera) must be redrawn.
    lv_subject_t* cube_revision_subject();
    lv_subject_t* status_subject();
    lv_subject_t* moves_subject();
    // True while the status/moves text is hidden and the cube sits in the middle.
    lv_subject_t* info_hidden_subject();
    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::AppPage current_page() const;
    void request_quit();

    // Cube scene, read by the cube view when drawing.
    const model::CubeModel& cube() const;
    char turning_move() const;      // 0 when no layer is turning
    float turning_angle() const;    // degrees, 0..90
    float camera_pitch() const;     // angleX in 3d/main.c
    float camera_yaw() const;       // angleY in 3d/main.c

    // Same keys as the GLUT version. Returns true when the key was used.
    bool handle_cube_key(uint32_t key);
    // Stops the camera rotation started by holding an arrow key.
    void handle_cube_key_release(uint32_t key);
    void turn(char move);
    void search(bool use_bfs);
    void play_solution();
    void orbit(float pitch_delta, float yaw_delta);

private:
    struct SearchJob;

    bool busy() const;
    void start_turn(char move);
    void start_next_solution_move();
    void on_turn_tick();
    void on_search_tick();
    void on_orbit_tick();
    void orbit_key(uint32_t key, float degrees);
    void set_solution(const char* moves, int count);
    void publish_moves();
    void bump_cube_revision();
    void publish_all();

    static void turn_timer_cb(lv_timer_t* timer);
    static void search_timer_cb(lv_timer_t* timer);
    static void orbit_timer_cb(lv_timer_t* timer);

    model::BaseModel model_;
    model::CubeModel cube_;
    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::BoolSubject quit_requested_subject_;
    reactive::IntSubject cube_revision_subject_;
    reactive::StringSubject<40> status_subject_;
    reactive::StringSubject<96> moves_subject_;
    reactive::BoolSubject info_hidden_subject_;

    float pitch_{30.0f};
    float yaw_{0.0f};
    uint32_t held_orbit_key_{0};
    uint32_t orbit_held_since_{0};
    char turning_move_{0};
    float turning_angle_{0.0f};
    bool playing_{false};
    std::deque<char> solution_;

    lv_timer_t* turn_timer_{nullptr};
    lv_timer_t* search_timer_{nullptr};
    lv_timer_t* orbit_timer_{nullptr};
    std::shared_ptr<SearchJob> job_;
    std::thread worker_;
};

} // namespace viewmodel
