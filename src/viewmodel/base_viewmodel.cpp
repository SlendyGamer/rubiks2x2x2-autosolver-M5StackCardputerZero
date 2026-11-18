/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "rubik_solver.h"

#include <array>
#include <string>

namespace viewmodel {
namespace {

// Same values as the GLUT version for the turn: 5 degrees every 20 ms.
constexpr uint32_t kTurnPeriodMs = 20;
constexpr float kTurnStepDegrees = 5.0f;
// One arrow tap moves the camera 15 degrees (360 is a multiple); holding it for longer than
// kOrbitHoldDelayMs keeps the camera rotating smoothly until the key is released.
constexpr float kCameraStepDegrees = 15.0f;
constexpr uint32_t kOrbitHoldDelayMs = 300;
constexpr float kOrbitHoldStepDegrees = 3.0f;
constexpr uint32_t kSearchPollMs = 50;
constexpr int kMaxSolutionMoves = 32;

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

} // namespace

struct BaseViewModel::SearchJob {
    std::array<char, RUBIK_STICKER_COUNT> pattern{};
    bool use_bfs{true};
    std::array<char, kMaxSolutionMoves> moves{};
    int count{-1};
    std::atomic<bool> done{false};
};

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false),
      cube_revision_subject_(0),
      status_subject_("Ready"),
      moves_subject_(""),
      info_hidden_subject_(false) {
    turn_timer_ = lv_timer_create(turn_timer_cb, kTurnPeriodMs, this);
    search_timer_ = lv_timer_create(search_timer_cb, kSearchPollMs, this);
    orbit_timer_ = lv_timer_create(orbit_timer_cb, kTurnPeriodMs, this);
    lv_timer_pause(turn_timer_);
    lv_timer_pause(search_timer_);
    lv_timer_pause(orbit_timer_);
}

BaseViewModel::~BaseViewModel() {
    // The search runs the original solver, which cannot be interrupted. The job is kept
    // alive by the thread itself, so leaving it running while the app exits is safe.
    if (worker_.joinable()) {
        worker_.detach();
    }
    if (turn_timer_) {
        lv_timer_delete(turn_timer_);
    }
    if (search_timer_) {
        lv_timer_delete(search_timer_);
    }
    if (orbit_timer_) {
        lv_timer_delete(orbit_timer_);
    }
}

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

lv_subject_t* BaseViewModel::cube_revision_subject() {
    return cube_revision_subject_.native();
}

lv_subject_t* BaseViewModel::status_subject() {
    return status_subject_.native();
}

lv_subject_t* BaseViewModel::moves_subject() {
    return moves_subject_.native();
}

lv_subject_t* BaseViewModel::info_hidden_subject() {
    return info_hidden_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

const model::CubeModel& BaseViewModel::cube() const {
    return cube_;
}

char BaseViewModel::turning_move() const {
    return turning_move_;
}

float BaseViewModel::turning_angle() const {
    return turning_angle_;
}

float BaseViewModel::camera_pitch() const {
    return pitch_;
}

float BaseViewModel::camera_yaw() const {
    return yaw_;
}

bool BaseViewModel::handle_cube_key(uint32_t key) {
    switch (key) {
        case 'x': case 'X':
        case 'y': case 'Y':
        case 'z': case 'Z':
            turn(static_cast<char>(key));
            return true;
        case 'b': case 'B':
            search(true);
            return true;
        case 'd': case 'D':
            search(false);
            return true;
        case 's': case 'S':
            play_solution();
            return true;
        case 'h': case 'H':
            info_hidden_subject_.toggle();
            return true;
        case LV_KEY_UP:
        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:
        case LV_KEY_LEFT:
            if (held_orbit_key_ != key) {
                orbit_key(key, kCameraStepDegrees);
                held_orbit_key_ = key;
                orbit_held_since_ = lv_tick_get();
                lv_timer_reset(orbit_timer_);
                lv_timer_resume(orbit_timer_);
            }
            return true;
        default:
            return false;
    }
}

void BaseViewModel::handle_cube_key_release(uint32_t key) {
    if (key == held_orbit_key_) {
        held_orbit_key_ = 0;
        lv_timer_pause(orbit_timer_);
    }
}

void BaseViewModel::on_orbit_tick() {
    if (held_orbit_key_ != 0 && lv_tick_elaps(orbit_held_since_) >= kOrbitHoldDelayMs) {
        orbit_key(held_orbit_key_, kOrbitHoldStepDegrees);
    }
}

void BaseViewModel::orbit_key(uint32_t key, float degrees) {
    switch (key) {
        case LV_KEY_UP:
            orbit(-degrees, 0.0f);
            break;
        case LV_KEY_DOWN:
            orbit(degrees, 0.0f);
            break;
        case LV_KEY_RIGHT:
            orbit(0.0f, degrees);
            break;
        case LV_KEY_LEFT:
            orbit(0.0f, -degrees);
            break;
        default:
            break;
    }
}

bool BaseViewModel::busy() const {
    return turning_move_ != 0 || job_ != nullptr;
}

void BaseViewModel::turn(char move) {
    if (busy()) {
        return;
    }
    // A manual turn invalidates the computed solution, like the original.
    solution_.clear();
    playing_ = false;
    publish_moves();
    start_turn(move);
}

void BaseViewModel::start_turn(char move) {
    turning_move_ = move;
    turning_angle_ = 0.0f;
    lv_timer_reset(turn_timer_);
    lv_timer_resume(turn_timer_);
    bump_cube_revision();
}

void BaseViewModel::on_turn_tick() {
    turning_angle_ += kTurnStepDegrees;
    if (turning_angle_ < 90.0f) {
        bump_cube_revision();
        return;
    }

    cube_.apply_move(turning_move_);
    turning_move_ = 0;
    turning_angle_ = 0.0f;

    if (playing_ && !solution_.empty()) {
        start_next_solution_move();
        return;
    }

    playing_ = false;
    lv_timer_pause(turn_timer_);
    status_subject_.set(cube_.is_solved() ? "Solved!" : "Ready");
    bump_cube_revision();
}

void BaseViewModel::search(bool use_bfs) {
    if (busy()) {
        return;
    }

    solution_.clear();
    playing_ = false;
    publish_moves();
    status_subject_.set(use_bfs ? "Searching (BFS)..." : "Searching (DFS)...");

    auto job = std::make_shared<SearchJob>();
    job->pattern = cube_.stickers();
    job->use_bfs = use_bfs;
    job_ = job;

    worker_ = std::thread([job] {
        job->count = rubik_solve(job->pattern.data(),
                                 job->use_bfs ? RUBIK_ALGO_BFS : RUBIK_ALGO_DFS,
                                 job->moves.data(),
                                 kMaxSolutionMoves);
        job->done.store(true);
    });

    lv_timer_reset(search_timer_);
    lv_timer_resume(search_timer_);
}

void BaseViewModel::on_search_tick() {
    if (!job_ || !job_->done.load()) {
        return;
    }

    lv_timer_pause(search_timer_);
    if (worker_.joinable()) {
        worker_.join();
    }

    const auto job = job_;
    job_.reset();

    if (job->count < 0) {
        status_subject_.set("No solution found");
    }
    else if (job->count == 0) {
        status_subject_.set("Already solved");
    }
    else {
        set_solution(job->moves.data(), job->count);
        const std::string text = "Found " + std::to_string(job->count) + " moves - S to play";
        status_subject_.set(text.c_str());
    }
}

void BaseViewModel::set_solution(const char* moves, int count) {
    solution_.assign(moves, moves + count);
    publish_moves();
}

void BaseViewModel::play_solution() {
    if (busy() || solution_.empty()) {
        return;
    }

    playing_ = true;
    status_subject_.set("Playing solution...");
    start_next_solution_move();
}

void BaseViewModel::start_next_solution_move() {
    const char move = solution_.front();
    solution_.pop_front();
    publish_moves();
    start_turn(move);
}

void BaseViewModel::publish_moves() {
    std::string text;
    for (const char move : solution_) {
        if (!text.empty()) {
            text += ' ';
        }
        text += move;
    }
    moves_subject_.set(text.c_str());
}

void BaseViewModel::orbit(float pitch_delta, float yaw_delta) {
    pitch_ += pitch_delta;
    yaw_ += yaw_delta;
    bump_cube_revision();
}

void BaseViewModel::bump_cube_revision() {
    cube_revision_subject_.set(cube_revision_subject_.value() + 1);
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

void BaseViewModel::turn_timer_cb(lv_timer_t* timer) {
    auto* view_model = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (view_model) {
        view_model->on_turn_tick();
    }
}

void BaseViewModel::search_timer_cb(lv_timer_t* timer) {
    auto* view_model = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (view_model) {
        view_model->on_search_tick();
    }
}

void BaseViewModel::orbit_timer_cb(lv_timer_t* timer) {
    auto* view_model = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (view_model) {
        view_model->on_orbit_tick();
    }
}

} // namespace viewmodel
